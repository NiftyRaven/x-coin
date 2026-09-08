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

#include <cstdint>
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
 *  - A node is lottery-eligible only if it links an X account (-xaccount)
 *    that appears on the operator-shared verified allowlist.
 *  - Eligible nodes heartbeat a payable script plus that X identity.
 *    Node id = Hash160(script). Heartbeats missing a verified link are
 *    ignored for the active set (one X account → one active node).
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
static const size_t MAX_X_HANDLE = 32;
static const char COMMIT_MAGIC[4] = {'X', 'H', 'B', '1'};

struct XAccount {
    std::string handle; // normalized lowercase, no leading '@'
    uint64_t userId;
    XAccount() : userId(0) {}
};

struct ActiveNode {
    uint160 id;
    CScript script;
    int64_t lastSeen;
    XAccount x;
};

struct VerifiedXAccount {
    std::string handle;
    uint64_t userId;
};

class Allowlist
{
public:
    void SetPersistPath(const std::string& path);
    std::string PersistPath() const;

    /** Add handle (and optional numeric X user id). Persists when a path is set. */
    bool Add(const std::string& handle, uint64_t userId, std::string& err, bool persist = true);
    bool Remove(const std::string& handle, std::string& err, bool persist = true);
    bool Contains(const std::string& handle, uint64_t userId = 0) const;
    std::vector<VerifiedXAccount> List() const;
    size_t Size() const;

    /** Merge accounts from a text file (handle [userid] per line, '#' comments). */
    bool LoadFile(const std::string& path, std::string& err);
    bool Persist(std::string& err) const;

private:
    mutable CCriticalSection cs;
    std::map<std::string, uint64_t> handles;
    std::map<uint64_t, std::string> byUserId;
    std::string persistPath;
};

class Registry
{
public:
    void SetLocalScript(const CScript& script);
    CScript LocalScript() const;
    uint160 LocalId() const;

    void SetLocalXAccount(const XAccount& account);
    XAccount LocalXAccount() const;
    // On a successful verified X-link, call AssignLinkedUserMainAsset (src/assets/xaccount.h).

    /**
     * Record a payable script as active if the X identity is linked and
     * allowlisted. id = Hash160(script). One verified X handle → one node.
     * Returns false (and does not insert) when the link is missing or unverified.
     */
    bool Heartbeat(const CScript& script, int64_t now,
                   const std::string& xHandle, uint64_t xUserId);
    bool HeartbeatLocal(int64_t now);

    bool LocalEligible() const;

    std::vector<uint160> ActiveIds(int64_t now) const;
    std::vector<ActiveNode> ActiveNodes(int64_t now) const;
    CScript ScriptFor(const uint160& id) const;
    size_t Count(int64_t now) const;

private:
    mutable CCriticalSection cs;
    std::map<uint160, ActiveNode> nodes;
    std::map<std::string, uint160> byHandle;
    CScript localScript;
    uint160 localId;
    XAccount localX;
};

Registry& GetRegistry();
Allowlist& GetAllowlist();

uint160 IdFromScript(const CScript& script);

/** Strip '@', lowercase, validate [a-z0-9_]{1,32}. */
bool NormalizeXHandle(const std::string& in, std::string& out, std::string& err);

/** Parse "handle" or "handle:userid" / "handle,userid". */
bool ParseXAccountSpec(const std::string& in, XAccount& out, std::string& err);

/** Load -xaccount / -xuserid / -xallowlist / -xverified and the datadir file. */
void InitEligibility();

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
