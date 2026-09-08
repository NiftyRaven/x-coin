// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pool.h"

#include "base58.h"
#include "crypto/sha256.h"
#include "hash.h"
#include "lottery.h"
#include "net.h"
#include "netmessagemaker.h"
#include "protocol.h"
#include "script/standard.h"
#include "support/cleanse.h"
#include "util.h"
#include "utilstrencodings.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <univalue.h>

#ifdef __linux__
#include <sys/stat.h>
#endif

namespace xpool {

static void RestrictPrivateFile(const fs::path& path)
{
#ifndef WIN32
    chmod(path.string().c_str(), S_IRUSR | S_IWUSR);
#endif
}

static fs::path SecretsPath()
{
    return GetDataDir() / "pools.secret.json";
}

static fs::path AdvertsPath()
{
    return GetDataDir() / "pools.json";
}

Registry& Get()
{
    static Registry g;
    return g;
}

static std::string ScriptToAddr(const CScript& script)
{
    CTxDestination dest;
    if (ExtractDestination(script, dest))
        return EncodeDestination(dest);
    return HexStr(script.begin(), script.end());
}

bool Registry::NormalizeId(const std::string& in, std::string& out, std::string& err) const
{
    out.clear();
    std::string s = in;
    while (!s.empty() && (s[0] == ' ' || s[0] == '\t'))
        s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    if (s.empty() || s.size() > MAX_POOL_ID) {
        err = "pool id must be 1–32 characters";
        return false;
    }
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            err = "pool id may contain letters, digits, _ and -";
            return false;
        }
        out.push_back(c);
    }
    return true;
}

bool Registry::NormalizeName(const std::string& in, std::string& out, std::string& err) const
{
    out.clear();
    std::string s = in;
    while (!s.empty() && (s[0] == ' ' || s[0] == '\t'))
        s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    if (s.empty() || s.size() > MAX_POOL_NAME) {
        err = "pool name must be 1–32 characters";
        return false;
    }
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 32 || c == 127) {
            err = "pool name has a control character";
            return false;
        }
    }
    out = s;
    return true;
}

void Registry::SortUniqueMembers(Advert& a) const
{
    std::map<uint160, CScript> byId;
    for (size_t i = 0; i < a.members.size(); i++) {
        if (a.members[i].empty())
            continue;
        byId[lottery::IdFromScript(a.members[i])] = a.members[i];
    }
    a.members.clear();
    for (std::map<uint160, CScript>::const_iterator it = byId.begin(); it != byId.end(); ++it)
        a.members.push_back(it->second);
}

uint256 Registry::AdvertDigest(const Advert& a) const
{
    uint256 out;
    unsigned char seqle[4];
    seqle[0] = a.seq & 0xff;
    seqle[1] = (a.seq >> 8) & 0xff;
    seqle[2] = (a.seq >> 16) & 0xff;
    seqle[3] = (a.seq >> 24) & 0xff;
    CSHA256 h;
    h.Write(reinterpret_cast<const unsigned char*>(ADVERT_MAGIC), sizeof(ADVERT_MAGIC) - 1);
    h.Write(reinterpret_cast<const unsigned char*>(a.id.data()), a.id.size());
    h.Write(reinterpret_cast<const unsigned char*>(a.name.data()), a.name.size());
    h.Write(seqle, 4);
    for (size_t i = 0; i < a.members.size(); i++) {
        const uint160 id = lottery::IdFromScript(a.members[i]);
        h.Write(id.begin(), 20);
    }
    h.Finalize(out.begin());
    return out;
}

bool Registry::DeriveKey(const std::string& poolId, const std::string& password, CKey& key) const
{
    if (password.empty() || password.size() > 128)
        return false;
    for (uint32_t n = 0; n < 1024; n++) {
        unsigned char secret[32];
        unsigned char nle[4];
        nle[0] = n & 0xff;
        nle[1] = (n >> 8) & 0xff;
        nle[2] = (n >> 16) & 0xff;
        nle[3] = (n >> 24) & 0xff;
        CSHA256()
            .Write(reinterpret_cast<const unsigned char*>(ADVERT_MAGIC), sizeof(ADVERT_MAGIC) - 1)
            .Write(reinterpret_cast<const unsigned char*>(poolId.data()), poolId.size())
            .Write(reinterpret_cast<const unsigned char*>(password.data()), password.size())
            .Write(nle, 4)
            .Finalize(secret);
        key.Set(secret, secret + 32, true);
        memory_cleanse(secret, sizeof(secret));
        if (key.IsValid())
            return true;
    }
    return false;
}

bool Registry::SignAdvert(Advert& a, const CKey& key) const
{
    a.sig.clear();
    if (!key.IsValid())
        return false;
    SortUniqueMembers(a);
    a.pub = key.GetPubKey();
    const uint256 digest = AdvertDigest(a);
    return key.SignCompact(digest, a.sig);
}

bool Registry::VerifyAdvert(const Advert& a) const
{
    if (a.id.empty() || a.name.empty() || !a.pub.IsValid())
        return false;
    if (a.members.empty() || a.members.size() > MAX_POOL_MEMBERS)
        return false;
    if (a.sig.empty() || a.sig.size() > 65)
        return false;
    Advert tmp = a;
    SortUniqueMembers(tmp);
    const uint256 digest = AdvertDigest(tmp);
    CPubKey rec;
    if (!rec.RecoverCompact(digest, a.sig))
        return false;
    return rec == a.pub;
}

bool Registry::PersistSecrets() const
{
    UniValue arr(UniValue::VARR);
    for (std::map<std::string, std::string>::const_iterator it = secrets.begin(); it != secrets.end(); ++it) {
        UniValue o(UniValue::VOBJ);
        o.pushKV("id", it->first);
        o.pushKV("password", it->second);
        arr.push_back(o);
    }
    UniValue root(UniValue::VOBJ);
    root.pushKV("secrets", arr);
    const std::string data = root.write(2);
    FILE* f = fsbridge::fopen(SecretsPath(), "wb");
    if (!f)
        return false;
    const bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    if (ok)
        RestrictPrivateFile(SecretsPath());
    return ok;
}

bool Registry::PersistAdverts() const
{
    UniValue arr(UniValue::VARR);
    for (std::map<std::string, Advert>::const_iterator it = adverts.begin(); it != adverts.end(); ++it) {
        const Advert& a = it->second;
        UniValue o(UniValue::VOBJ);
        o.pushKV("id", a.id);
        o.pushKV("name", a.name);
        o.pushKV("seq", (int64_t)a.seq);
        o.pushKV("pubkey", HexStr(a.pub.begin(), a.pub.end()));
        o.pushKV("sig", HexStr(a.sig.begin(), a.sig.end()));
        UniValue mem(UniValue::VARR);
        for (size_t i = 0; i < a.members.size(); i++)
            mem.push_back(ScriptToAddr(a.members[i]));
        o.pushKV("addresses", mem);
        arr.push_back(o);
    }
    UniValue root(UniValue::VOBJ);
    root.pushKV("adverts", arr);
    const std::string data = root.write(2);
    FILE* f = fsbridge::fopen(AdvertsPath(), "wb");
    if (!f)
        return false;
    const bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    if (ok)
        RestrictPrivateFile(AdvertsPath());
    return ok;
}

void Registry::Load()
{
    adverts.clear();
    secrets.clear();
    {
        std::ifstream in(SecretsPath().string().c_str());
        std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        UniValue u;
        if (u.read(s) && u.isObject() && u.exists("secrets") && u["secrets"].isArray()) {
            const UniValue& arr = u["secrets"];
            for (size_t i = 0; i < arr.size(); i++) {
                const UniValue& o = arr[i];
                if (!o.isObject())
                    continue;
                secrets[o["id"].getValStr()] = o["password"].getValStr();
            }
        }
    }
    {
        std::ifstream in(AdvertsPath().string().c_str());
        std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        UniValue u;
        if (u.read(s) && u.isObject() && u.exists("adverts") && u["adverts"].isArray()) {
            const UniValue& arr = u["adverts"];
            for (size_t i = 0; i < arr.size(); i++) {
                const UniValue& o = arr[i];
                if (!o.isObject())
                    continue;
                Advert a;
                a.id = o["id"].getValStr();
                a.name = o["name"].getValStr();
                a.seq = (uint32_t)atoi(o["seq"].getValStr().c_str());
                std::vector<unsigned char> pub = ParseHex(o["pubkey"].getValStr());
                a.pub.Set(pub.begin(), pub.end());
                a.sig = ParseHex(o["sig"].getValStr());
                const UniValue& mem = o["addresses"];
                if (mem.isArray()) {
                    for (size_t j = 0; j < mem.size(); j++) {
                        const CTxDestination dest = DecodeDestination(mem[j].getValStr());
                        if (IsValidDestination(dest))
                            a.members.push_back(GetScriptForDestination(dest));
                    }
                }
                std::string err;
                if (VerifyAdvert(a))
                    adverts[a.id] = a;
            }
        }
    }
}

void Registry::Init()
{
    LOCK(cs);
    Load();
}

void Registry::Reset()
{
    LOCK(cs);
    adverts.clear();
    secrets.clear();
}

bool Registry::HasSecret(const std::string& poolId) const
{
    LOCK(cs);
    return secrets.count(poolId) > 0;
}

std::string Registry::LocalPoolId() const
{
    LOCK(cs);
    const CScript local = lottery::GetRegistry().LocalScript();
    if (!local.empty()) {
        const uint160 id = lottery::IdFromScript(local);
        for (std::map<std::string, Advert>::const_iterator it = adverts.begin(); it != adverts.end(); ++it) {
            for (size_t i = 0; i < it->second.members.size(); i++) {
                if (lottery::IdFromScript(it->second.members[i]) == id)
                    return it->first;
            }
        }
    }
    // Wallet still knows the password even if the lottery payout script moved.
    if (secrets.size() == 1)
        return secrets.begin()->first;
    return std::string();
}

bool Registry::GetById(const std::string& poolId, Advert& out) const
{
    std::string id, err;
    if (!NormalizeId(poolId, id, err))
        return false;
    LOCK(cs);
    std::map<std::string, Advert>::const_iterator it = adverts.find(id);
    if (it == adverts.end())
        return false;
    out = it->second;
    return true;
}

bool Registry::GetByMember(const uint160& memberId, Advert& out) const
{
    LOCK(cs);
    const Advert* best = nullptr;
    uint32_t bestSeq = 0;
    for (std::map<std::string, Advert>::const_iterator it = adverts.begin(); it != adverts.end(); ++it) {
        for (size_t i = 0; i < it->second.members.size(); i++) {
            if (lottery::IdFromScript(it->second.members[i]) == memberId) {
                if (!best || it->second.seq >= bestSeq) {
                    best = &it->second;
                    bestSeq = it->second.seq;
                }
            }
        }
    }
    if (!best)
        return false;
    out = *best;
    return true;
}

std::vector<Advert> Registry::AllAdverts() const
{
    LOCK(cs);
    std::vector<Advert> v;
    for (std::map<std::string, Advert>::const_iterator it = adverts.begin(); it != adverts.end(); ++it)
        v.push_back(it->second);
    return v;
}

int Registry::EligibleTickets(const Advert& a, const std::vector<uint160>& activeIds) const
{
    int n = 0;
    for (size_t i = 0; i < a.members.size(); i++) {
        const uint160 id = lottery::IdFromScript(a.members[i]);
        if (std::find(activeIds.begin(), activeIds.end(), id) != activeIds.end())
            n++;
    }
    return n;
}

void Registry::PushAdvertToPeers(const Advert& a) const
{
    if (!g_connman || a.id.empty() || a.sig.empty() || !a.pub.IsValid())
        return;
    std::vector<unsigned char> pub(a.pub.begin(), a.pub.end());
    g_connman->ForEachNode([&](CNode* pto) {
        if (!pto->fSuccessfullyConnected || pto->fDisconnect)
            return;
        CNetMsgMaker msgMaker(pto->GetSendVersion());
        g_connman->PushMessage(pto, msgMaker.Make(NetMsgType::XPL, a.id, a.name, a.seq,
                                                  pub, a.members, a.sig));
    });
}

bool Registry::AcceptGossip(const Advert& in, std::string& err)
{
    if (!VerifyAdvert(in)) {
        err = "invalid pool advert";
        return false;
    }
    Advert mergedPush;
    bool pushMerged = false;
    {
        LOCK(cs);
        std::map<std::string, Advert>::iterator it = adverts.find(in.id);
        if (it == adverts.end()) {
            if (adverts.size() >= MAX_POOLS) {
                err = "too many pools";
                return false;
            }
            adverts[in.id] = in;
            PersistAdverts();
            return true;
        }
        if (it->second.pub != in.pub) {
            err = "pool id already used with a different password";
            return false;
        }
        if (secrets.count(in.id)) {
            Advert u = it->second;
            std::map<uint160, CScript> ours;
            std::map<uint160, CScript> incoming;
            for (size_t i = 0; i < u.members.size(); i++)
                ours[lottery::IdFromScript(u.members[i])] = u.members[i];
            for (size_t i = 0; i < in.members.size(); i++)
                incoming[lottery::IdFromScript(in.members[i])] = in.members[i];
            bool dropped = false;
            for (std::map<uint160, CScript>::const_iterator m = ours.begin(); m != ours.end(); ++m) {
                if (!incoming.count(m->first))
                    dropped = true;
            }
            std::map<uint160, CScript> byId = ours;
            for (std::map<uint160, CScript>::const_iterator m = incoming.begin(); m != incoming.end(); ++m)
                byId[m->first] = m->second;
            // Higher seq that drops a member is a leave — do not union the leaver back in.
            if (dropped && in.seq > u.seq)
                byId = incoming;
            std::vector<CScript> merged;
            for (std::map<uint160, CScript>::const_iterator m = byId.begin(); m != byId.end(); ++m)
                merged.push_back(m->second);
            if (merged.size() > MAX_POOL_MEMBERS) {
                err = "pool is full";
                return false;
            }
            const bool grew = merged.size() > u.members.size();
            const bool shrunk = merged.size() < u.members.size();
            if (grew || shrunk || in.seq > u.seq) {
                CKey key;
                if (!DeriveKey(in.id, secrets[in.id], key))
                    return false;
                u.members = merged;
                if (u.name == u.id && in.name != in.id)
                    u.name = in.name;
                else if (in.name != in.id && u.name != in.name && in.seq >= u.seq)
                    u.name = in.name;
                u.seq = in.seq > u.seq ? in.seq : u.seq;
                if (grew)
                    u.seq += 1;
                if (!SignAdvert(u, key))
                    return false;
                it->second = u;
                PersistAdverts();
                if (grew || shrunk) {
                    mergedPush = u;
                    pushMerged = true;
                }
            }
        } else {
            if (in.seq < it->second.seq)
                return true;
            if (in.seq == it->second.seq && in.members.size() <= it->second.members.size())
                return true;
            it->second = in;
            PersistAdverts();
        }
    }
    if (pushMerged)
        PushAdvertToPeers(mergedPush);
    return true;
}

bool Registry::ApplyMembership(Advert& a, const CScript& localScript, bool add, std::string& err)
{
    SortUniqueMembers(a);
    const uint160 localId = lottery::IdFromScript(localScript);
    std::vector<CScript> next;
    bool found = false;
    for (size_t i = 0; i < a.members.size(); i++) {
        const uint160 id = lottery::IdFromScript(a.members[i]);
        if (id == localId) {
            found = true;
            if (add)
                next.push_back(a.members[i]);
        } else {
            next.push_back(a.members[i]);
        }
    }
    if (add && !found)
        next.push_back(localScript);
    if (!add && !found) {
        err = "you are not in this pool";
        return false;
    }
    if (add && next.empty()) {
        err = "pool would have no members";
        return false;
    }
    if (next.size() > MAX_POOL_MEMBERS) {
        err = "pool is full";
        return false;
    }
    a.members = next;
    a.seq += 1;
    SortUniqueMembers(a);
    return true;
}

bool Registry::Create(const std::string& name, const std::string& poolId, const std::string& password,
                      const CScript& localScript, std::string& err)
{
    std::string id, nm;
    if (!NormalizeId(poolId, id, err) || !NormalizeName(name, nm, err))
        return false;
    if (password.empty() || password.size() > 128) {
        err = "set a password (1–128 characters). Share it only with people you want in the pool.";
        return false;
    }
    if (localScript.empty()) {
        err = "open a wallet first";
        return false;
    }
    CKey key;
    if (!DeriveKey(id, password, key)) {
        err = "could not derive pool key";
        return false;
    }
    const std::string already = LocalPoolId();
    if (!already.empty() && already != id) {
        err = "leave your current pool before creating another";
        return false;
    }
    Advert created;
    {
        LOCK(cs);
        if (adverts.count(id)) {
            err = "that pool id already exists on this node";
            return false;
        }
        if (adverts.size() >= MAX_POOLS) {
            err = "too many pools";
            return false;
        }
        Advert a;
        a.id = id;
        a.name = nm;
        a.seq = 1;
        a.members.push_back(localScript);
        if (!SignAdvert(a, key)) {
            err = "could not sign pool";
            return false;
        }
        adverts[id] = a;
        secrets[id] = password;
        PersistSecrets();
        PersistAdverts();
        created = a;
    }
    PushAdvertToPeers(created);
    return true;
}

bool Registry::Join(const std::string& poolId, const std::string& password,
                    const CScript& localScript, std::string& err)
{
    std::string id;
    if (!NormalizeId(poolId, id, err))
        return false;
    if (password.empty()) {
        err = "password required";
        return false;
    }
    if (localScript.empty()) {
        err = "open a wallet first";
        return false;
    }
    CKey key;
    if (!DeriveKey(id, password, key)) {
        err = "could not derive pool key";
        return false;
    }
    const std::string already = LocalPoolId();
    if (!already.empty() && already != id) {
        err = "leave your current pool before joining another";
        return false;
    }
    Advert joined;
    {
        LOCK(cs);
        Advert a;
        if (adverts.count(id)) {
            a = adverts[id];
            if (a.pub != key.GetPubKey()) {
                err = "wrong password";
                return false;
            }
        } else {
            a.id = id;
            a.name = id;
            a.seq = 0;
        }
        if (!ApplyMembership(a, localScript, true, err))
            return false;
        if (a.name.empty())
            a.name = id;
        if (!SignAdvert(a, key)) {
            err = "could not sign join";
            return false;
        }
        adverts[id] = a;
        secrets[id] = password;
        PersistSecrets();
        PersistAdverts();
        joined = a;
    }
    PushAdvertToPeers(joined);
    return true;
}

bool Registry::Leave(const std::string& poolId, const std::string& password,
                     const CScript& localScript, std::string& err)
{
    std::string id;
    if (!NormalizeId(poolId, id, err))
        return false;
    CKey key;
    Advert left;
    {
        LOCK(cs);
        std::string pass = password;
        if (pass.empty() && secrets.count(id))
            pass = secrets[id];
        if (pass.empty()) {
            err = "password required to leave";
            return false;
        }
        if (!DeriveKey(id, pass, key)) {
            err = "could not derive pool key";
            return false;
        }
        if (!adverts.count(id)) {
            err = "unknown pool";
            return false;
        }
        Advert a = adverts[id];
        if (a.pub != key.GetPubKey()) {
            err = "wrong password";
            return false;
        }
        if (!ApplyMembership(a, localScript, false, err))
            return false;
        secrets.erase(id);
        if (a.members.empty()) {
            adverts.erase(id);
            PersistSecrets();
            PersistAdverts();
            return true;
        }
        if (!SignAdvert(a, key)) {
            err = "could not sign leave";
            return false;
        }
        adverts[id] = a;
        PersistSecrets();
        PersistAdverts();
        left = a;
    }
    PushAdvertToPeers(left);
    return true;
}

CScript MakePoolCommitment(const std::vector<std::vector<uint160> >& groups)
{
    std::vector<unsigned char> data;
    data.push_back((unsigned char)COMMIT_MAGIC[0]);
    data.push_back((unsigned char)COMMIT_MAGIC[1]);
    data.push_back((unsigned char)COMMIT_MAGIC[2]);
    data.push_back((unsigned char)COMMIT_MAGIC[3]);
    if (groups.size() > 16)
        return CScript();
    data.push_back((unsigned char)groups.size());
    for (size_t g = 0; g < groups.size(); g++) {
        if (groups[g].size() > MAX_POOL_MEMBERS || groups[g].empty())
            return CScript();
        data.push_back((unsigned char)groups[g].size());
        for (size_t i = 0; i < groups[g].size(); i++)
            data.insert(data.end(), groups[g][i].begin(), groups[g][i].end());
    }
    return CScript() << OP_RETURN << data;
}

bool ParsePoolCommitment(const CScript& script, std::vector<std::vector<uint160> >& groups)
{
    groups.clear();
    if (script.empty() || script[0] != OP_RETURN)
        return false;
    CScript::const_iterator pc = script.begin() + 1;
    std::vector<unsigned char> data;
    opcodetype opcode;
    if (!script.GetOp(pc, opcode, data))
        return false;
    if (data.size() < 5)
        return false;
    if (data[0] != (unsigned char)COMMIT_MAGIC[0] ||
        data[1] != (unsigned char)COMMIT_MAGIC[1] ||
        data[2] != (unsigned char)COMMIT_MAGIC[2] ||
        data[3] != (unsigned char)COMMIT_MAGIC[3])
        return false;
    const unsigned nGroups = data[4];
    if (nGroups > 16)
        return false;
    size_t off = 5;
    groups.resize(nGroups);
    for (unsigned g = 0; g < nGroups; g++) {
        if (off >= data.size())
            return false;
        const unsigned n = data[off++];
        if (n == 0 || n > MAX_POOL_MEMBERS)
            return false;
        if (off + (size_t)n * 20 > data.size())
            return false;
        groups[g].resize(n);
        for (unsigned i = 0; i < n; i++) {
            memcpy(groups[g][i].begin(), &data[off], 20);
            off += 20;
        }
        for (unsigned i = 1; i < n; i++) {
            if (!(groups[g][i - 1] < groups[g][i]))
                return false;
        }
    }
    return off == data.size();
}

} // namespace xpool
