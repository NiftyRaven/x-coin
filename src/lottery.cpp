// Copyright (c) 2026 The X Coin developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "lottery.h"

#include "chain.h"
#include "chainparams.h"
#include "consensus/validation.h"
#include "fs.h"
#include "hash.h"
#include "miner.h"
#include "primitives/block.h"
#include "pubkey.h"
#include "random.h"
#include "script/standard.h"
#include "util.h"
#include "utilstrencodings.h"
#include "validation.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
extern std::vector<CWalletRef> vpwallets;
#endif

#include <boost/bind/bind.hpp>
#include <boost/thread.hpp>

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

bool Allowlist::Add(const std::string& handle, uint64_t userId, std::string& err, bool persist)
{
    std::string norm;
    if (!NormalizeXHandle(handle, norm, err))
        return false;
    {
        LOCK(cs);
        handles[norm] = userId;
        if (userId != 0)
            byUserId[userId] = norm;
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
            err = "X handle is not on the verified allowlist";
            return false;
        }
        if (it->second != 0)
            byUserId.erase(it->second);
        handles.erase(it);
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
    const bool haveHandle = NormalizeXHandle(handle, norm, err);
    LOCK(cs);
    if (haveHandle) {
        auto it = handles.find(norm);
        if (it != handles.end())
            return true;
    }
    if (userId != 0) {
        auto it = byUserId.find(userId);
        if (it != byUserId.end())
            return true;
    }
    return false;
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
        err = strprintf("cannot open verified X allowlist %s", path);
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
        // File lines: "handle" or "handle userid" (space) or handle:userid
        std::string spec = t;
        size_t sp = t.find_first_of(" \t");
        if (sp != std::string::npos && t.find_first_of(":,") == std::string::npos) {
            spec = t.substr(0, sp) + ":" + TrimCopy(t.substr(sp + 1));
        }
        if (!ParseXAccountSpec(spec, acc, perr)) {
            err = strprintf("%s (line %s)", perr, t);
            return false;
        }
        std::string aerr;
        if (!Add(acc.handle, acc.userId, aerr, false)) {
            err = aerr;
            return false;
        }
        n++;
    }
    err.clear();
    LogPrintf("lottery: loaded %d verified X account(s) from %s\n", n, path);
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
            cur.push_back(a);
        }
    }
    FILE* f = fsbridge::fopen(path, "wb");
    if (!f) {
        err = strprintf("cannot write verified X allowlist %s", path);
        return false;
    }
    fprintf(f, "# X Coin verified X accounts (operator-shared allowlist)\n");
    fprintf(f, "# Only list X-verified (blue-check / X Premium verified) handles.\n");
    fprintf(f, "# format: handle [userid]\n");
    for (const auto& a : cur) {
        if (a.userId != 0)
            fprintf(f, "%s %llu\n", a.handle.c_str(), (unsigned long long)a.userId);
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

    XAccount local;
    std::string err;
    if (gArgs.IsArgSet("-xaccount")) {
        if (!NormalizeXHandle(gArgs.GetArg("-xaccount", ""), local.handle, err))
            LogPrintf("lottery: -xaccount: %s\n", err);
    }
    const int64_t uidArg = gArgs.GetArg("-xuserid", 0);
    if (uidArg > 0)
        local.userId = (uint64_t)uidArg;
    GetRegistry().SetLocalXAccount(local);

    if (local.handle.empty()) {
        LogPrintf("lottery: no -xaccount; this node will sync/relay but is not lottery-eligible\n");
    } else if (!allow.Contains(local.handle, local.userId)) {
        LogPrintf("lottery: -xaccount=%s is not on the verified allowlist; sync/relay only\n",
                  local.handle);
    } else {
        LogPrintf("lottery: linked verified X account %s (userid=%llu); lottery-eligible\n",
                  local.handle, (unsigned long long)local.userId);
    }
}

uint160 IdFromScript(const CScript& script)
{
    return Hash160(script);
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
    return GetAllowlist().Contains(x.handle, x.userId);
}

bool Registry::Heartbeat(const CScript& script, int64_t now,
                         const std::string& xHandle, uint64_t xUserId)
{
    if (script.empty() || script.size() > MAX_HEARTBEAT_SCRIPT)
        return false;
    const uint160 id = IdFromScript(script);
    if (id.IsNull())
        return false;
    std::string norm;
    std::string err;
    if (!NormalizeXHandle(xHandle, norm, err))
        return false;
    if (!GetAllowlist().Contains(norm, xUserId))
        return false;

    LOCK(cs);
    auto hit = byHandle.find(norm);
    if (hit != byHandle.end() && hit->second != id) {
        nodes.erase(hit->second);
        byHandle.erase(hit);
    }
    auto nit = nodes.find(id);
    int64_t joined = now;
    if (nit != nodes.end()) {
        if (nit->second.x.handle != norm)
            byHandle.erase(nit->second.x.handle);
        else
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

bool Registry::HeartbeatLocal(int64_t now)
{
    CScript s;
    XAccount x;
    {
        LOCK(cs);
        s = localScript;
        x = localX;
    }
    if (s.empty() || x.handle.empty())
        return false;
    return Heartbeat(s, now, x.handle, x.userId);
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
    fs::path path = GetDataDir() / "lottery-payout.dat";
    if (fs::exists(path)) {
        FILE* f = fsbridge::fopen(path, "rb");
        if (f) {
            if (fseek(f, 0, SEEK_END) == 0) {
                long sz = ftell(f);
                if (sz > 0 && sz <= (long)MAX_HEARTBEAT_SCRIPT && fseek(f, 0, SEEK_SET) == 0) {
                    std::vector<unsigned char> buf((size_t)sz);
                    size_t n = fread(buf.data(), 1, buf.size(), f);
                    fclose(f);
                    if (n == buf.size())
                        return CScript(buf.begin(), buf.end());
                } else {
                    fclose(f);
                }
            } else {
                fclose(f);
            }
        }
    }

    uint256 rnd = GetRandHash();
    uint160 h;
    CHash160().Write(rnd.begin(), 32).Finalize(h.begin());
    CScript script = GetScriptForDestination(CKeyID(h));

    FILE* f = fsbridge::fopen(path, "wb");
    if (f) {
        fwrite(script.data(), 1, script.size(), f);
        fclose(f);
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
        tx.vout.emplace_back(value, dest);
    }
    tx.vout.emplace_back(0, MakeActiveSetCommitment(draw.active));
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
    for (const auto& out : cb.vout) {
        if (ParseActiveSetCommitment(out.scriptPubKey, committed)) {
            found = true;
            break;
        }
    }
    if (!found)
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-commit", false,
                         "coinbase missing XHB1 active-set commitment");

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
    if (pays.size() != winners.size())
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-count", false,
                         strprintf("expected %u winner outputs, got %u",
                                   (unsigned)winners.size(), (unsigned)pays.size()));

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

    std::vector<CAmount> parts = SplitReward(subsidy, (int)winners.size());
    for (size_t i = 0; i < winners.size(); i++) {
        if (IdFromScript(pays[i].scriptPubKey) != winners[i])
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-winner", false,
                             strprintf("payout %u script does not match winner", (unsigned)i));
        CAmount expect = parts[i];
        if (i == 0)
            expect += (paid - subsidy);
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
    if (!GetRegistry().LocalEligible()) {
        LogPrintf("lottery: refusing to produce; local node is not linked+verified\n");
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

    std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(chainparams).CreateNewBlock(coinbaseScript->reserveScript));
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
    bool fLoggedIneligible = false;

    try {
        while (true) {
            boost::this_thread::interruption_point();
            const int64_t now = GetTime();

#ifdef ENABLE_WALLET
            // Adopt the wallet mining script once. Do not touch the keypool
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

            if (!GetRegistry().LocalEligible()) {
                if (!fLoggedIneligible) {
                    LogPrintf("lottery: not linked to a verified X account; sync/relay only, not producing\n");
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

            if (currentSlot < slot) {
                MilliSleep(1000);
                continue;
            }
            // Let heartbeats settle so honest nodes freeze the same set.
            if (now < slot * SLOT_SECONDS + SETTLE_SECONDS) {
                MilliSleep(200);
                continue;
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

            // Only winners[0] produces. Other drawn ids are payees on that coinbase.
            if (!draw.winners.empty() && GetRegistry().LocalId() == draw.winners[0]) {
                if (ProduceOneBlock(chainparams))
                    lastProducedSlot = slot;
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
