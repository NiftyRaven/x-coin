// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_POOL_H
#define XCOIN_POOL_H

#include "key.h"
#include "pubkey.h"
#include "script/script.h"
#include "sync.h"
#include "uint256.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/**
 * Optional lottery pools.
 *
 * Tickets: each X Verified eligible member still heartbeats as one node,
 * so the pool's chance to win is the count of verified running members.
 * Unverified members have zero tickets.
 *
 * Payout: when a pool member wins a slot, that winner's subsidy is split
 * evenly across every member payout address (verified or not). Sharing
 * pool id + password is how someone joins; keep them private to stop leeches.
 *
 * Public display: pool name + member payout addresses only.
 */
namespace xpool {

static const size_t MAX_POOLS = 256;
static const size_t MAX_POOL_MEMBERS = 64;
static const size_t MAX_POOL_ID = 32;
static const size_t MAX_POOL_NAME = 32;
static const char COMMIT_MAGIC[4] = {'X', 'P', 'L', '1'};
static const char ADVERT_MAGIC[] = "xcoin-xpl-v1";

struct Advert {
    std::string id;
    std::string name;
    uint32_t seq;
    CPubKey pub;
    std::vector<CScript> members;
    std::vector<unsigned char> sig;
    Advert() : seq(0) {}
};

class Registry
{
public:
    void Init();
    void Reset();

    bool Create(const std::string& name, const std::string& poolId, const std::string& password,
                const CScript& localScript, std::string& err);
    bool Join(const std::string& poolId, const std::string& password,
              const CScript& localScript, std::string& err);
    bool Leave(const std::string& poolId, const std::string& password,
               const CScript& localScript, std::string& err);

    bool HasSecret(const std::string& poolId) const;
    std::string LocalPoolId() const;
    bool GetById(const std::string& poolId, Advert& out) const;
    bool GetByMember(const uint160& memberId, Advert& out) const;
    std::vector<Advert> AllAdverts() const;

    bool AcceptGossip(const Advert& in, std::string& err);
    int EligibleTickets(const Advert& a, const std::vector<uint160>& activeIds) const;
    void PushAdvertToPeers(const Advert& a) const;

    bool DeriveKey(const std::string& poolId, const std::string& password, CKey& key) const;
    bool SignAdvert(Advert& a, const CKey& key) const;
    bool VerifyAdvert(const Advert& a) const;

private:
    bool NormalizeId(const std::string& in, std::string& out, std::string& err) const;
    bool NormalizeName(const std::string& in, std::string& out, std::string& err) const;
    uint256 AdvertDigest(const Advert& a) const;
    void SortUniqueMembers(Advert& a) const;
    bool PersistSecrets() const;
    bool PersistAdverts() const;
    void Load();
    bool ApplyMembership(Advert& a, const CScript& localScript, bool add, std::string& err);

    mutable CCriticalSection cs;
    std::map<std::string, Advert> adverts;
    std::map<std::string, std::string> secrets; // id -> password
};

Registry& Get();

CScript MakePoolCommitment(const std::vector<std::vector<uint160> >& groups);
bool ParsePoolCommitment(const CScript& script, std::vector<std::vector<uint160> >& groups);

} // namespace xpool

#endif
