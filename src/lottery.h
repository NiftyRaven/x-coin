// Copyright (c) 2026 The X Coin developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_LOTTERY_H
#define XCOIN_LOTTERY_H

#include "amount.h"
#include "script/script.h"
#include "sync.h"
#include "uint256.h"

#include <map>
#include <string>
#include <vector>

class CBlock;
class CChainParams;
class CValidationState;

/**
 * X Coin lottery consensus.
 *
 * Design (see docs/LOTTERY.md):
 *  - Every running node is an active node after a heartbeat that advertises
 *    a payable script. Node id = Hash160(script).
 *  - Honest peers gossip `xhb` messages so they share one active-node set.
 *  - One lottery slot per minute; height maps 1:1 with slots after genesis.
 *  - Winner count starts at 1 and increases by 1 at each subsidy halving.
 *  - Winners are a deterministic sample of the sorted active-node set,
 *    seeded from the previous block hash and the slot number.
 *  - The slot subsidy is split among winners; fees go to the first winner.
 *  - The producer commits the sorted active ids in a coinbase OP_RETURN
 *    (`XHB1`) and validation rejects a coinbase that does not pay that draw.
 */
namespace lottery {

static const int64_t SLOT_SECONDS = 60;
static const int64_t HEARTBEAT_TTL_SECONDS = 180;
static const size_t MAX_ACTIVE_NODES = 4096;
static const size_t MAX_HEARTBEAT_SCRIPT = 520;
static const char COMMIT_MAGIC[4] = {'X', 'H', 'B', '1'};

struct ActiveNode {
    uint160 id;
    CScript script;
    int64_t lastSeen;
};

class Registry
{
public:
    void SetLocalScript(const CScript& script);
    CScript LocalScript() const;
    uint160 LocalId() const;

    /** Record a payable script as active. id = Hash160(script). */
    void Heartbeat(const CScript& script, int64_t now);
    void HeartbeatLocal(int64_t now);

    std::vector<uint160> ActiveIds(int64_t now) const;
    std::vector<ActiveNode> ActiveNodes(int64_t now) const;
    CScript ScriptFor(const uint160& id) const;
    size_t Count(int64_t now) const;

private:
    mutable CCriticalSection cs;
    std::map<uint160, ActiveNode> nodes;
    CScript localScript;
    uint160 localId;
};

Registry& GetRegistry();

uint160 IdFromScript(const CScript& script);

/** Persist or create a stable local payout script in the data directory. */
CScript LoadOrCreateLocalScript();

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

/** Replace coinbase vout with winner payouts + XHB1 active-set commitment. */
void ApplyCoinbasePayouts(CBlock& block,
                          int nHeight,
                          const uint256& prevBlockHash,
                          int64_t genesisTime,
                          int nSubsidyHalvingInterval,
                          CAmount subsidy,
                          CAmount nFees,
                          const CScript& producerScript);

/** Enforce committed active set and multi-winner payouts (height >= 1). */
bool CheckLotteryCoinbase(const CBlock& block,
                          int nHeight,
                          const uint256& prevBlockHash,
                          int64_t genesisTime,
                          int nSubsidyHalvingInterval,
                          CAmount subsidy,
                          CValidationState& state);

CScript MakeActiveSetCommitment(const std::vector<uint160>& sortedIds);
bool ParseActiveSetCommitment(const CScript& script, std::vector<uint160>& ids);

void StartProducer(const CChainParams& chainparams);
void StopProducer();
bool IsProducerRunning();

} // namespace lottery

#endif // XCOIN_LOTTERY_H
