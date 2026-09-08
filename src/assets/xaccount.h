// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_ASSETS_XACCOUNT_H
#define XCOIN_ASSETS_XACCOUNT_H

#if defined(HAVE_CONFIG_H)
#include "config/raven-config.h"
#endif

#include "amount.h"

#include <string>

class CScript;
class CTransaction;
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

bool CheckIfXAccountAssigned(const std::string& xId, std::string* assetName = nullptr);
bool AddXAccountAssignment(const std::string& xId, const std::string& assetName);
bool RemoveXAccountAssignment(const std::string& xId);
bool LoadXAccountAssignments();

#ifdef ENABLE_WALLET
/** Call on a successful X-link. Idempotent. dest empty → new wallet address. */
bool AssignLinkedUserMainAsset(const std::string& xHandleOrId, const std::string& dest,
                               std::string& err, std::string* assetNameOut = nullptr,
                               std::string* txidOut = nullptr);
#endif

#endif
