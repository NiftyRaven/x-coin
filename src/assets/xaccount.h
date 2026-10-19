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
#include "pubkey.h"
#include "uint256.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

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
 * Site proof (XPF1) on an identity claim. Policy only: the second push of the
 * same OP_RETURN. ParseXAccountAssignmentScript still reads only the first
 * push, so 1.0.17–1.0.22 nodes parse the claim the same way. Not consensus.
 *
 * Payload: "XPF1" | ver u8=1 | keyId u8 | userId u64 LE | expiry u32 LE | sig 65.
 * Digest: SHA256d("xcoin-xpf-v1" || u8 len+handle || userId u64 LE ||
 * hash160 of the P2PKH that receives the root and owner outputs ||
 * u8 len+root name || expiry u32 LE || keyId u8).
 * Any configured signer pubkey may verify. Enforcement is off when none are set.
 */
static const unsigned char XCLAIM_PROOF_VERSION = 1;
static const unsigned int XCLAIM_PROOF_PAYLOAD_SIZE = 4 + 1 + 1 + 8 + 4 + 65;
static const int64_t XCLAIM_PROOF_MAX_FUTURE = 3600;
/** Hard cap for a structurally valid XPF1 script. Above MAX_OP_RETURN_RELAY. */
static const unsigned int MAX_XCLAIM_DATACARRIER_BYTES = 160;

struct XClaimProof {
    uint8_t keyId;
    uint64_t userId;
    uint32_t expiry;
    std::vector<unsigned char> sig;

    XClaimProof() : keyId(0), userId(0), expiry(0) {}
};

bool BuildXClaimProofPayload(uint8_t keyId, uint64_t userId, uint32_t expiry, const std::vector<unsigned char>& sig, std::vector<unsigned char>& out);
CScript MakeXAccountAssignmentScriptWithProof(const std::string& xId, const std::vector<unsigned char>& proofPayload);
/** True when the XID1 output carries a structurally valid XPF1 second push. */
bool ParseXClaimProof(const CTransaction& tx, XClaimProof& proof);
uint256 XClaimProofDigest(const std::string& handle, uint64_t userId, const uint160& addrHash, const std::string& root, uint32_t expiry, uint8_t keyId);
/**
 * Reasons: xclaim-proof-missing, xclaim-proof-bad-sig, xclaim-proof-expired,
 * xclaim-proof-mismatch. Expiry must be > now and <= now+3600.
 * Unknown key id and a bad signature are both xclaim-proof-bad-sig.
 * Uses the configured signer set. Enforcement off skips this in the mempool.
 */
bool CheckXClaimProof(const CTransaction& tx, int64_t now, std::string& err);

/** Empty -xclaimproofpubkey means enforcement is off. */
bool XClaimProofEnforced();
const std::map<uint8_t, CPubKey>& XClaimProofPubs();
/**
 * Each entry is "id:66hex" or a comma list of those. Repeatable.
 * Duplicate id is an error. Empty list turns enforcement off.
 * Format only: curve check is XClaimProofPubReady, after ECC_Start.
 */
bool SetXClaimProofPubkeys(const std::vector<std::string>& specs, std::string& err);
/** Call after ECC_Start. Rejects an off-curve signer pubkey. */
bool XClaimProofPubReady(std::string& err);
void SetXClaimProofPubsForTest(const std::map<uint8_t, CPubKey>& pubs);
/**
 * Signer URL list. Explicit -xclaimproofurls (including empty) wins.
 * Unset on mainnet defaults to the live site, then the seed standby port.
 * Unset on regtest and testnet is empty, so local claims stay unproofed.
 */
std::vector<std::string> XClaimProofUrls();
/** True when a new identity claim must carry XPF1 before broadcast. */
bool XClaimProofShouldAttach();
typedef bool (*XClaimHttpPostFn)(const std::string& url, const std::string& bearer, const std::string& body, int& httpStatus, std::string& response, std::string& err);
void SetXClaimHttpPostForTest(XClaimHttpPostFn fn);
/** Parse a signer JSON body into an XPF1 payload. No network. */
bool XClaimProofFromSignerJson(const std::string& json, const std::string& handle, uint64_t userId, const std::string& address, const std::string& root, int64_t now, std::vector<unsigned char>& payload, std::string& err);
/** POST the claim fields to the URL list and attach a proof. Does not broadcast. */
bool FetchXClaimProof(const std::string& handle, uint64_t userId, const std::string& address, const std::string& root, std::vector<unsigned char>& payload, std::string& err);
/** Policy: skip this tx in the block template when enforcement rejects it. */
bool XClaimProofBlocksInclusion(const CTransaction& tx);
/** True when an oversized null-data script is an identity claim with structural XPF1. */
bool XClaimDatacarrierExempt(const CTransaction& tx, const CScript& scriptPubKey);
/** Shown when the signer cannot be reached. Coins, Launch, and existing names keep working. */
const char* XClaimSignerUnreachableMessage();
const char* XClaimSignInAgainMessage();
std::string FormatXClaimWalletError(const std::string& raw);

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
/** Real disconnect / reorg only. VerifyDB dry-run must not call this. */
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
