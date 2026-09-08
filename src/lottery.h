// Copyright (c) 2026 The X Coin developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_LOTTERY_H
#define XCOIN_LOTTERY_H

#include "amount.h"
#include "sync.h"
#include "uint256.h"

#include <map>
#include <string>
#include <vector>

class CChainParams;

/**
 * X Coin lottery consensus (Phase 1 scaffold).
 *
 * Design (see docs/LOTTERY.md):
 *  - Every running node is an active node after a heartbeat.
 *  - One lottery slot per minute; height maps 1:1 with slots after genesis.
 *  - Winner count starts at 1 and increases by 1 at each subsidy halving.
 *  - Winners are a deterministic sample of the sorted active-node set,
 *    seeded from the previous block hash and the slot number.
 *  - The slot subsidy is split as evenly as possible among that minute's winners.
 *
 * Phase 1 implements the algorithm, in-memory registry, producer thread, and
 * RPCs. Cross-node heartbeat gossip and coinbase multi-pay enforcement are
 * Phase 2.
 */
namespace lottery {

static const int64_t SLOT_SECONDS = 60;
static const int64_t HEARTBEAT_TTL_SECONDS = 180;

struct ActiveNode {
    uint160 id;
    int64_t lastSeen;
};

class Registry
{
public:
    void SetLocalId(const uint160& id);
    uint160 LocalId() const;

    void Heartbeat(const uint160& id, int64_t now);
    void HeartbeatLocal(int64_t now);

    std::vector<uint160> ActiveIds(int64_t now) const;
    std::vector<ActiveNode> ActiveNodes(int64_t now) const;
    size_t Count(int64_t now) const;

private:
    mutable CCriticalSection cs;
    std::map<uint160, int64_t> nodes;
    uint160 localId;
};

Registry& GetRegistry();

/** Persist or create a stable local node id in the data directory. */
uint160 LoadOrCreateLocalId();

/** Unix minute of a unix timestamp (floor). */
int64_t SlotFromTime(int64_t unixTime);

/** Height 0 is genesis (no lottery). Height n maps to genesisSlot + n. */
int64_t SlotFromHeight(int nHeight, int64_t genesisTime);

/** 1 winner before the first halving, 2 after the first, 3 after the second, … */
int WinnerCount(int nHeight, int nSubsidyHalvingInterval);

/**
 * Deterministic seed: SHA256(prevBlockHash || little-endian slot).
 * Same inputs ⇒ same seed on every honest node.
 */
uint256 Seed(const uint256& prevBlockHash, int64_t slot);

/**
 * Pick k distinct winners from a sorted active-id list using seed as a
 * deterministic RNG. If k >= n, every active node wins.
 */
std::vector<uint160> SelectWinners(const std::vector<uint160>& sortedActive,
                                   const uint256& seed,
                                   int k);

bool IsWinner(const uint160& id, const std::vector<uint160>& winners);

/** Split total into k parts; remainder satoshis go to the first winners. */
std::vector<CAmount> SplitReward(CAmount total, int winners);

struct Draw {
    int nHeight;
    int64_t slot;
    int winnerCount;
    uint256 seed;
    std::vector<uint160> active;
    std::vector<uint160> winners;
    std::vector<CAmount> rewards;
};

/** Compute the draw for the next block on top of prevHash at nNextHeight. */
Draw ComputeDraw(int nNextHeight,
                 const uint256& prevBlockHash,
                 int64_t genesisTime,
                 int nSubsidyHalvingInterval,
                 CAmount subsidy,
                 int64_t now);

void StartProducer(const CChainParams& chainparams);
void StopProducer();
bool IsProducerRunning();

} // namespace lottery

#endif // XCOIN_LOTTERY_H
