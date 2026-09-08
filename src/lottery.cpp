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
#include <cstring>
#include <ctime>

namespace lottery {

static Registry g_registry;
static boost::thread_group* g_producerThreads = nullptr;
static CCriticalSection cs_producer;

Registry& GetRegistry()
{
    return g_registry;
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

void Registry::Heartbeat(const CScript& script, int64_t now)
{
    if (script.empty() || script.size() > MAX_HEARTBEAT_SCRIPT)
        return;
    const uint160 id = IdFromScript(script);
    if (id.IsNull())
        return;
    LOCK(cs);
    nodes[id] = ActiveNode{id, script, now};
    if (nodes.size() > MAX_ACTIVE_NODES) {
        auto oldest = nodes.begin();
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            if (it->second.lastSeen < oldest->second.lastSeen)
                oldest = it;
        }
        if (oldest->first != localId)
            nodes.erase(oldest);
    }
}

void Registry::HeartbeatLocal(int64_t now)
{
    CScript s = LocalScript();
    if (!s.empty())
        Heartbeat(s, now);
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
    d.active = GetRegistry().ActiveIds(now);
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
        if (ids[i] <= ids[i - 1])
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
    if (!producerScript.empty()) {
        GetRegistry().SetLocalScript(producerScript);
        GetRegistry().Heartbeat(producerScript, now);
    } else {
        GetRegistry().HeartbeatLocal(now);
    }

    Draw draw = ComputeDraw(nHeight, prevBlockHash, genesisTime,
                            nSubsidyHalvingInterval, subsidy, now);
    if (draw.winners.empty()) {
        // Should not happen after heartbeating the producer script.
        GetRegistry().Heartbeat(producerScript, now);
        draw = ComputeDraw(nHeight, prevBlockHash, genesisTime,
                           nSubsidyHalvingInterval, subsidy, now);
    }
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
    for (const auto& p : pays)
        paid += p.nValue;
    if (paid < subsidy)
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
    (void)IsWitnessCommitment; // used only as documentation of skipped OP_RETURNs
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
#ifdef ENABLE_WALLET
    CWallet* pWallet = FirstWalletOrNull();
    if (!pWallet) {
        LogPrintf("lottery: no wallet; cannot produce a block\n");
        return false;
    }
    std::shared_ptr<CReserveScript> coinbaseScript;
    pWallet->GetScriptForMining(coinbaseScript);
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

    try {
        while (true) {
            boost::this_thread::interruption_point();
            const int64_t now = GetTime();

#ifdef ENABLE_WALLET
            CWallet* pWallet = FirstWalletOrNull();
            if (pWallet) {
                std::shared_ptr<CReserveScript> coinbaseScript;
                pWallet->GetScriptForMining(coinbaseScript);
                if (coinbaseScript && !coinbaseScript->reserveScript.empty())
                    GetRegistry().SetLocalScript(coinbaseScript->reserveScript);
            }
#endif
            GetRegistry().HeartbeatLocal(now);

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
            if (slot == lastProducedSlot) {
                MilliSleep(1000);
                continue;
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

            if (IsWinner(GetRegistry().LocalId(), draw.winners)) {
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
