// Copyright (c) 2026 The X Coin developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "lottery.h"

#include "chain.h"
#include "chainparams.h"
#include "fs.h"
#include "hash.h"
#include "miner.h"
#include "random.h"
#include "util.h"
#include "utilstrencodings.h"
#include "validation.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
extern std::vector<CWalletRef> vpwallets;
#endif

#include <boost/bind.hpp>
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

void Registry::SetLocalId(const uint160& id)
{
    LOCK(cs);
    localId = id;
}

uint160 Registry::LocalId() const
{
    LOCK(cs);
    return localId;
}

void Registry::Heartbeat(const uint160& id, int64_t now)
{
    if (id.IsNull())
        return;
    LOCK(cs);
    nodes[id] = now;
}

void Registry::HeartbeatLocal(int64_t now)
{
    Heartbeat(LocalId(), now);
}

std::vector<uint160> Registry::ActiveIds(int64_t now) const
{
    std::vector<uint160> ids;
    LOCK(cs);
    ids.reserve(nodes.size());
    for (const auto& kv : nodes) {
        if (now - kv.second <= HEARTBEAT_TTL_SECONDS)
            ids.push_back(kv.first);
    }
    // std::map is already sorted by uint160
    return ids;
}

std::vector<ActiveNode> Registry::ActiveNodes(int64_t now) const
{
    std::vector<ActiveNode> out;
    LOCK(cs);
    out.reserve(nodes.size());
    for (const auto& kv : nodes) {
        if (now - kv.second <= HEARTBEAT_TTL_SECONDS)
            out.push_back({kv.first, kv.second});
    }
    return out;
}

size_t Registry::Count(int64_t now) const
{
    return ActiveIds(now).size();
}

uint160 LoadOrCreateLocalId()
{
    fs::path path = GetDataDir() / "lottery-nodeid.dat";
    if (fs::exists(path)) {
        FILE* f = fsbridge::fopen(path, "rb");
        if (f) {
            unsigned char buf[20];
            size_t n = fread(buf, 1, 20, f);
            fclose(f);
            if (n == 20) {
                uint160 id;
                memcpy(id.begin(), buf, 20);
                if (!id.IsNull())
                    return id;
            }
        }
    }

    uint256 rnd = GetRandHash();
    uint160 id;
    CHash160().Write(rnd.begin(), 32).Finalize(id.begin());

    FILE* f = fsbridge::fopen(path, "wb");
    if (f) {
        fwrite(id.begin(), 1, 20, f);
        fclose(f);
    }
    return id;
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
    // One extra winner per completed halving. Cap so we cannot overflow
    // a pathological height and so a tiny active set still terminates.
    if (halvings > 1023)
        halvings = 1023;
    return halvings + 1;
}

uint256 Seed(const uint256& prevBlockHash, int64_t slot)
{
    uint256 out;
    uint64_t le = 0;
    // little-endian slot for host-independent hashing
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
    // Keep winner list in selection order (first winner is the block producer).
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

    GetRegistry().SetLocalId(LoadOrCreateLocalId());
    int64_t lastProducedSlot = -1;

    try {
        while (true) {
            boost::this_thread::interruption_point();
            const int64_t now = GetTime();
            GetRegistry().HeartbeatLocal(now);

            // Regtest / mine-on-demand: heartbeat only; blocks come from RPC.
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

            // One block per minute: wait until wall-clock slot catches the height slot.
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
                // Solo / first node: if the set is empty we still registered
                // ourselves this loop; retry next second.
                MilliSleep(1000);
                continue;
            }

            if (IsWinner(GetRegistry().LocalId(), draw.winners) ||
                draw.winners.front() == GetRegistry().LocalId()) {
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
    GetRegistry().SetLocalId(LoadOrCreateLocalId());
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
