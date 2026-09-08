// Copyright (c) 2026 The X Coin developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_LOTTERY_H
#define XCOIN_LOTTERY_H

#include "amount.h"
#include "key.h"
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
 *  - Fair lottery: nobody who is X Verified (X blue check / X Premium, or
 *    business / government org check from GET /2/users/me) and running a
 *    wallet can be excluded. The operator invite list is not a lottery
 *    gate. Typed -xaccount= is not enough.
 *  - A session that is not X Verified has zero chance (no heartbeat, not
 *    in the active set) so unverified accounts cannot exploit the draw.
 *  - Eligible nodes heartbeat a payable script plus that X identity.
 *    Node id = Hash160(script). Heartbeats missing a verified link are
 *    ignored for the active set (one X account → one active node).
 *  - Gossip `xhb` must be compact-signed by the payout key. A live
 *    handle cannot be rebound to another script (stops reward theft).
 *  - Honest peers gossip signed `xhb` messages so they share one set.
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
/** How many future slots a header may sit in vs the local clock (clock skew). */
static const int64_t MAX_FUTURE_SLOTS = 1;
static const int64_t HEARTBEAT_TTL_SECONDS = 180;
static const size_t MAX_ACTIVE_NODES = 4096;
static const size_t MAX_HEARTBEAT_SCRIPT = 520;
static const size_t MAX_HEARTBEAT_SIG = 65;
static const size_t MAX_X_HANDLE = 32;
static const char COMMIT_MAGIC[4] = {'X', 'H', 'B', '1'};
static const char HEARTBEAT_MAGIC[] = "xcoin-xhb-v2";

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
    CScript pinnedScript; // empty = no operator pin
};

class Allowlist
{
public:
    void SetPersistPath(const std::string& path);
    std::string PersistPath() const;

    /** Add handle (and optional numeric X user id / payout pin). Persists when a path is set. */
    bool Add(const std::string& handle, uint64_t userId, std::string& err, bool persist = true,
             const CScript& pinnedScript = CScript());
    bool Remove(const std::string& handle, std::string& err, bool persist = true);
    /**
     * True only if `handle` is listed. A userid alone is not enough
     * (closes the “steal listed userid, any handle” bypass).
     * If the list records a userid and the caller supplies one, they must match.
     */
    bool Contains(const std::string& handle, uint64_t userId = 0) const;
    /**
     * Invite list is not a lottery gate. X Verified (users/me) is.
     * Always true for a valid handle; payout pins still apply via PinnedScript.
     */
    bool Allows(const std::string& handle, uint64_t userId = 0) const;
    CScript PinnedScript(const std::string& handle) const;
    std::vector<VerifiedXAccount> List() const;
    size_t Size() const;
    /** Test helper: drop in-memory entries (does not erase the file). */
    void Reset();

    /** Merge accounts from a text file (handle [userid] per line, '#' comments). */
    bool LoadFile(const std::string& path, std::string& err);
    bool Persist(std::string& err) const;

private:
    mutable CCriticalSection cs;
    std::map<std::string, uint64_t> handles;
    std::map<uint64_t, std::string> byUserId;
    std::map<std::string, CScript> pins;
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
    // On a successful signed-in X-link, call AssignLinkedUserMainAsset (src/assets/xaccount.h).

    /**
     * Record a payable script as active. `xVerified` must be true (X blue
     * check). Unverified → false (zero chance). Not gated by the operator
     * invite list. id = Hash160(script). One X handle → one node.
     */
    bool Heartbeat(const CScript& script, int64_t now,
                   const std::string& xHandle, uint64_t xUserId, bool xVerified = true);
    bool HeartbeatLocal(int64_t now);

    bool LocalEligible() const;
    /** Test helper: drop the in-memory active set (keeps local script / X). */
    void Reset();

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

/** Compact-sig digest over timestamp + script + handle + userid. */
uint256 HeartbeatDigest(int64_t timestamp, const CScript& script,
                        const std::string& handle, uint64_t userId, bool xVerified);
bool SignHeartbeat(const CKey& key, int64_t timestamp, const CScript& script,
                   const std::string& handle, uint64_t userId,
                   std::vector<unsigned char>& sigOut, bool xVerified = true);
/** True if `sig` recovers a pubkey whose P2PKH script equals `script`. */
bool VerifyHeartbeatSig(const CScript& script, int64_t timestamp,
                        const std::string& handle, uint64_t userId,
                        const std::vector<unsigned char>& sig, bool xVerified = true);
/** Sign this node's payout + session handle for P2P. Numeric X user id is not signed onto the wire. */
bool SignLocalHeartbeat(int64_t timestamp, std::vector<unsigned char>& sigOut);

/** Parse "handle" or "handle:userid" / "handle,userid". */
bool ParseXAccountSpec(const std::string& in, XAccount& out, std::string& err);

/** Load allowlist + Sign in with X session (not a typed handle). */
void InitEligibility();

/** Persist or create a stable local payout script in the data directory. */
CScript LoadOrCreateLocalScript();

/** Unix minute of a unix timestamp (floor). */
int64_t SlotFromTime(int64_t unixTime);

/** Height 0 is genesis (no lottery). Height n maps to genesisSlot + n. */
int64_t SlotFromHeight(int nHeight, int64_t genesisTime);

/** Inclusive start of the locked 60-second window for this height. */
int64_t SlotStartTime(int nHeight, int64_t genesisTime);

/**
 * Permanent mainnet/testnet lock: one height per compile-time 60-second slot.
 * Slot length is not a P2P field and cannot be changed by a peer message.
 *
 * Rejects blocks whose nTime is outside [slotStart, slotStart+60) for that
 * height, and slots more than MAX_FUTURE_SLOTS ahead of the local clock
 * (stops minting many blocks by advancing the clock). Header nTime is part of
 * the block hash, so rewriting a timestamp after accept is a different block
 * and must still satisfy this rule.
 *
 * fMineBlocksOnDemand (regtest) skips the lock so generatetoaddress can
 * assemble blocks immediately.
 */
bool CheckBlockTime(int nHeight, int64_t nTime, int64_t genesisTime,
                    int64_t nowLocal, bool fMineBlocksOnDemand,
                    CValidationState& state);

/** Clamp a producer timestamp into the locked slot for nHeight. */
int64_t ClampTimeToSlot(int nHeight, int64_t genesisTime, int64_t now, int64_t mtp);

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
