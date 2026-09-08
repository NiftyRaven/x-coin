// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_ASSETS_XACCOUNT_H
#define XCOIN_ASSETS_XACCOUNT_H

#if defined(HAVE_CONFIG_H)
#include "config/raven-config.h"
#endif

#include "amount.h"
#include "primitives/transaction.h"

#include <string>

class CScript;
class CAssetsCache;

/**
 * Each signed-in X account is one main/root asset ("tokenize yourself").
 * Users cannot issue new roots. Only AssignLinkedUserMainAsset may create
 * a root (requires Sign in with X), and that issue burns 0 XFER.
 *
 * Circulating supply: 1 whole unit of NAME (units=0) plus owner token NAME!.
 */
static const CAmount MAIN_ASSET_CIRCULATING_AMOUNT = 1 * COIN;
static const char XACCOUNT_ASSIGN_MAGIC[4] = {'X', 'I', 'D', '1'};

bool NormalizeXAccountId(const std::string& in, std::string& handleOut, uint64_t& userIdOut, std::string& err);
/** Map an X handle to a candidate root name (uppercase, explicit edge rules).
 *  Does not assign or check uniqueness. Fails rather than truncate. */
bool MapXHandleToRootName(const std::string& handle, std::string& outName, std::string& err);
bool DeriveMainAssetName(const std::string& xHandleOrId, std::string& outName, std::string& err, CAssetsCache* cache = nullptr);

CScript MakeXAccountAssignmentScript(const std::string& xId);
bool ParseXAccountAssignmentScript(const CScript& script, std::string& xId);
bool ParseXAccountAssignment(const CTransaction& tx, std::string& xId);

/**
 * Zero-fee identity-root claims cannot use an empty vin (BIP144 treats
 * vin=[] as the witness marker). They also must not spend or create XFER.
 *
 * Consensus-safe dummy prevout: hash = SHA256d("xcoin-xid-vin-v1" || handle),
 * n = 0x58494431 ('XID1'). That n is not a real vout index, the hash is not a
 * coinbase null, and each X handle has its own outpoint so mempool conflicts
 * work like a double-spend. Never inserted into the UTXO set (not a premine).
 */
static const uint32_t XACCOUNT_DUMMY_N = 0x58494431u;

COutPoint MakeXAccountDummyPrevout(const std::string& normalizedHandle);
bool IsXAccountDummyPrevout(const COutPoint& prevout);
/** True when vin[nIn] is the dummy bound to this tx's XID1 handle. */
bool IsXAccountDummyInput(const CTransaction& tx, unsigned int nIn);
/**
 * Free identity-root assignment: XID1 + new root, exactly one dummy vin,
 * 0 XFER in/out, no witness. Not a coin grant.
 */
bool IsXAccountIdentityClaim(const CTransaction& tx, std::string* handleOut = nullptr);
/** Reject dummy prevouts that are not a valid identity claim. */
bool CheckXAccountDummyInputs(const CTransaction& tx, std::string& err);

bool CheckIfXAccountAssigned(const std::string& xId, std::string* assetName = nullptr);
bool AddXAccountAssignment(const std::string& xId, const std::string& assetName);
bool RemoveXAccountAssignment(const std::string& xId);
bool LoadXAccountAssignments();

/** Top-level root of NAME, NAME/CHILD, or NAME#tag. Empty if the name is invalid. */
std::string TopLevelRootName(const std::string& assetName);
/** Session required. Subs/uniques must sit under this handle’s assigned main asset. */
bool RequireIssueUnderOwnMain(const std::string& assetName, std::string& err);

#ifdef ENABLE_WALLET
/** Call on a successful X-link. Idempotent. dest empty → new wallet address. */
bool AssignLinkedUserMainAsset(const std::string& xHandleOrId, const std::string& dest,
                               std::string& err, std::string* assetNameOut = nullptr,
                               std::string* txidOut = nullptr);
#endif

#endif
