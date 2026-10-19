// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assets/xaccount.h"

#include "assets/assetdb.h"
#include "assets/assets.h"
#include "base58.h"
#include "crypto/common.h"
#include "hash.h"
#include "key.h"
#include "lottery.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "txmempool.h"
#include "chainparams.h"
#include "chainparamsbase.h"
#include "compat.h"
#include "support/cleanse.h"
#include "univalue.h"
#include "util.h"
#include "utilstrencodings.h"
#include "validation.h"
#include "xsession.h"

#ifdef ENABLE_WALLET
#include "rpc/protocol.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"
#endif

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sys/time.h>
#include <vector>

#include <boost/variant/get.hpp>

extern CAssetsDB* passetsdb;

static CCriticalSection cs_xaccount;
static std::map<std::string, std::string> mapXAccountToAsset;
static std::map<std::string, std::string> mapAssetToXAccount;

bool NormalizeXAccountId(const std::string& in, std::string& handleOut, uint64_t& userIdOut, std::string& err)
{
    handleOut.clear();
    userIdOut = 0;
    lottery::XAccount acc;
    if (!lottery::ParseXAccountSpec(in, acc, err))
        return false;
    handleOut = acc.handle;
    userIdOut = acc.userId;
    return true;
}

static std::string CollapseUnderscores(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    char prev = 0;
    for (char c : in) {
        if (c == '_' && prev == '_')
            continue;
        out.push_back(c);
        prev = c;
    }
    return out;
}

bool MapXHandleToRootName(const std::string& handle, std::string& outName, std::string& err)
{
    outName.clear();
    std::string out;
    out.reserve(handle.size() + 2);
    for (unsigned char c : handle) {
        if (c >= 'a' && c <= 'z')
            out.push_back(static_cast<char>(c - 'a' + 'A'));
        else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
            out.push_back(static_cast<char>(c));
        else {
            err = "X handle contains a character that cannot map to a root asset name";
            return false;
        }
    }
    out = CollapseUnderscores(out);
    // Asset names cannot start or end with `_`. X handles can. Replace the
    // edge underscore with X so the mapping is explicit and 1:1 in length
    // for the rest of the handle (tested; not a silent strip).
    if (!out.empty() && out.front() == '_')
        out.front() = 'X';
    if (!out.empty() && out.back() == '_')
        out.back() = 'X';
    out = CollapseUnderscores(out);
    while (out.size() < MIN_ASSET_LENGTH)
        out.insert(out.begin(), 'X');
    if (out.size() > MAX_ROOT_NAME_LENGTH) {
        err = strprintf("handle-derived root would exceed %d characters (no truncation)", MAX_ROOT_NAME_LENGTH);
        return false;
    }
    if (!IsAssetNameARoot(out)) {
        err = strprintf("handle-derived name %s is not a valid root", out);
        return false;
    }
    outName = out;
    return true;
}

static bool NameTaken(const std::string& name, CAssetsCache* cache)
{
    if (cache && cache->CheckIfAssetExists(name, true))
        return true;
    auto* live = GetCurrentAssetCache();
    if (live && live != cache && live->CheckIfAssetExists(name, true))
        return true;
    {
        LOCK(mempool.cs);
        if (mempool.mapAssetToHash.count(name))
            return true;
    }
    return false;
}

bool DeriveMainAssetName(const std::string& xHandleOrId, std::string& outName, std::string& err, CAssetsCache* cache)
{
    outName.clear();
    std::string handle;
    uint64_t userId = 0;
    if (!NormalizeXAccountId(xHandleOrId, handle, userId, err))
        return false;

    std::string base;
    if (!MapXHandleToRootName(handle, base, err))
        return false;
    auto tryName = [&](const std::string& n) -> bool {
        if (n.size() < MIN_ASSET_LENGTH || n.size() > MAX_ROOT_NAME_LENGTH)
            return false;
        if (!IsAssetNameARoot(n))
            return false;
        if (NameTaken(n, cache))
            return false;
        return true;
    };

    if (tryName(base)) {
        outName = base;
        return true;
    }

    std::vector<std::string> suffixes;
    if (userId != 0)
        suffixes.push_back(std::to_string(userId));
    for (int i = 2; i < 10000; i++)
        suffixes.push_back(std::to_string(i));

    for (const auto& suf : suffixes) {
        // Never truncate a valid handle-derived base. Skip suffixes that
        // would push the name over MAX_ROOT_NAME_LENGTH.
        if (base.size() + 1 + suf.size() > MAX_ROOT_NAME_LENGTH)
            continue;
        std::string n = base + "_" + suf;
        if (tryName(n)) {
            outName = n;
            return true;
        }
    }

    err = "unable to derive a unique main-asset name from X handle";
    return false;
}

CScript MakeXAccountAssignmentScript(const std::string& xId)
{
    std::vector<unsigned char> data;
    data.reserve(4 + xId.size());
    data.push_back((unsigned char)XACCOUNT_ASSIGN_MAGIC[0]);
    data.push_back((unsigned char)XACCOUNT_ASSIGN_MAGIC[1]);
    data.push_back((unsigned char)XACCOUNT_ASSIGN_MAGIC[2]);
    data.push_back((unsigned char)XACCOUNT_ASSIGN_MAGIC[3]);
    data.insert(data.end(), xId.begin(), xId.end());
    return CScript() << OP_RETURN << data;
}

bool ParseXAccountAssignmentScript(const CScript& script, std::string& xId)
{
    xId.clear();
    if (script.empty() || script[0] != OP_RETURN)
        return false;
    CScript::const_iterator pc = script.begin() + 1;
    std::vector<unsigned char> data;
    opcodetype opcode;
    if (!script.GetOp(pc, opcode, data))
        return false;
    if (data.size() < 5)
        return false;
    if (data[0] != (unsigned char)XACCOUNT_ASSIGN_MAGIC[0] ||
        data[1] != (unsigned char)XACCOUNT_ASSIGN_MAGIC[1] ||
        data[2] != (unsigned char)XACCOUNT_ASSIGN_MAGIC[2] ||
        data[3] != (unsigned char)XACCOUNT_ASSIGN_MAGIC[3])
        return false;
    xId.assign(data.begin() + 4, data.end());
    if (xId.empty() || xId.size() > lottery::MAX_X_HANDLE)
        return false;
    return true;
}

COutPoint MakeXAccountDummyPrevout(const std::string& normalizedHandle)
{
    CHash256 hasher;
    static const char tag[] = "xcoin-xid-vin-v1";
    hasher.Write(reinterpret_cast<const unsigned char*>(tag), sizeof(tag) - 1);
    hasher.Write(reinterpret_cast<const unsigned char*>(normalizedHandle.data()), normalizedHandle.size());
    uint256 hash;
    hasher.Finalize(hash.begin());
    return COutPoint(hash, XACCOUNT_DUMMY_N);
}

bool IsXAccountDummyPrevout(const COutPoint& prevout)
{
    return prevout.n == XACCOUNT_DUMMY_N && !prevout.hash.IsNull();
}

bool IsXAccountIdentityClaim(const CTransaction& tx, std::string* handleOut)
{
    if (handleOut)
        handleOut->clear();
    if (tx.IsCoinBase() || tx.HasWitness())
        return false;
    if (tx.vin.size() != 1)
        return false;
    if (tx.vin[0].prevout.IsNull() || !IsXAccountDummyPrevout(tx.vin[0].prevout))
        return false;
    if (!tx.vin[0].scriptSig.empty() || tx.vin[0].scriptWitness.stack.size() != 0)
        return false;
    if (tx.vin[0].nSequence != CTxIn::SEQUENCE_FINAL)
        return false;

    for (const auto& out : tx.vout) {
        if (out.nValue != 0)
            return false;
    }

    std::string xid;
    if (!ParseXAccountAssignment(tx, xid))
        return false;
    std::string handle;
    uint64_t uid = 0;
    std::string err;
    if (!NormalizeXAccountId(xid, handle, uid, err))
        return false;
    if (tx.vin[0].prevout != MakeXAccountDummyPrevout(handle))
        return false;
    if (!tx.IsNewAsset())
        return false;
    if (handleOut)
        *handleOut = handle;
    return true;
}

bool IsXAccountDummyInput(const CTransaction& tx, unsigned int nIn)
{
    if (nIn >= tx.vin.size())
        return false;
    if (!IsXAccountDummyPrevout(tx.vin[nIn].prevout))
        return false;
    return IsXAccountIdentityClaim(tx);
}

bool CheckXAccountDummyInputs(const CTransaction& tx, std::string& err)
{
    err.clear();
    unsigned int dummyCount = 0;
    for (const auto& in : tx.vin) {
        if (IsXAccountDummyPrevout(in.prevout))
            dummyCount++;
    }
    if (dummyCount == 0)
        return true;
    if (tx.IsCoinBase()) {
        err = "bad-txns-coinbase-xid-dummy";
        return false;
    }
    if (!IsXAccountIdentityClaim(tx)) {
        err = "bad-txns-xid-dummy-not-identity";
        return false;
    }
    if (dummyCount != 1) {
        err = "bad-txns-xid-dummy-count";
        return false;
    }
    return true;
}

bool ParseXAccountAssignment(const CTransaction& tx, std::string& xId)
{
    for (const auto& out : tx.vout) {
        if (ParseXAccountAssignmentScript(out.scriptPubKey, xId))
            return true;
    }
    return false;
}

static std::map<uint8_t, CPubKey> g_xclaimProofPubs;
static XClaimHttpPostFn g_xclaimHttpPost = nullptr;

static const char* XCLAIM_URL_PRIMARY = "https://xferchain.net/api/x/claim-proof";
static const char* XCLAIM_URL_FALLBACK = "http://172.191.195.221:38445/claim-proof";

bool BuildXClaimProofPayload(uint8_t keyId, uint64_t userId, uint32_t expiry, const std::vector<unsigned char>& sig, std::vector<unsigned char>& out)
{
    out.clear();
    if (sig.size() != 65)
        return false;
    out.assign(XCLAIM_PROOF_PAYLOAD_SIZE, 0);
    out[0] = 'X';
    out[1] = 'P';
    out[2] = 'F';
    out[3] = '1';
    out[4] = XCLAIM_PROOF_VERSION;
    out[5] = keyId;
    WriteLE64(&out[6], userId);
    WriteLE32(&out[14], static_cast<uint32_t>(expiry));
    std::memcpy(&out[18], sig.data(), 65);
    return true;
}

CScript MakeXAccountAssignmentScriptWithProof(const std::string& xId, const std::vector<unsigned char>& proofPayload)
{
    std::vector<unsigned char> data;
    data.reserve(4 + xId.size());
    data.push_back((unsigned char)XACCOUNT_ASSIGN_MAGIC[0]);
    data.push_back((unsigned char)XACCOUNT_ASSIGN_MAGIC[1]);
    data.push_back((unsigned char)XACCOUNT_ASSIGN_MAGIC[2]);
    data.push_back((unsigned char)XACCOUNT_ASSIGN_MAGIC[3]);
    data.insert(data.end(), xId.begin(), xId.end());
    return CScript() << OP_RETURN << data << proofPayload;
}

static bool ParseXClaimProofPayload(const std::vector<unsigned char>& data, XClaimProof& proof)
{
    proof = XClaimProof();
    if (data.size() != XCLAIM_PROOF_PAYLOAD_SIZE)
        return false;
    if (data[0] != 'X' || data[1] != 'P' || data[2] != 'F' || data[3] != '1')
        return false;
    if (data[4] != XCLAIM_PROOF_VERSION)
        return false;
    proof.keyId = data[5];
    proof.userId = ReadLE64(&data[6]);
    proof.expiry = ReadLE32(&data[14]);
    proof.sig.assign(data.begin() + 18, data.end());
    return proof.sig.size() == 65;
}

bool ParseXClaimProof(const CTransaction& tx, XClaimProof& proof)
{
    proof = XClaimProof();
    for (const auto& out : tx.vout) {
        std::string xid;
        if (!ParseXAccountAssignmentScript(out.scriptPubKey, xid))
            continue;
        const CScript& script = out.scriptPubKey;
        CScript::const_iterator pc = script.begin();
        opcodetype opcode;
        std::vector<unsigned char> first;
        if (!script.GetOp(pc, opcode, first) || opcode != OP_RETURN)
            return false;
        if (!script.GetOp(pc, opcode, first))
            return false;
        std::vector<unsigned char> second;
        if (!script.GetOp(pc, opcode, second))
            return false;
        if (pc != script.end())
            return false;
        return ParseXClaimProofPayload(second, proof);
    }
    return false;
}

uint256 XClaimProofDigest(const std::string& handle, uint64_t userId, const uint160& addrHash, const std::string& root, uint32_t expiry, uint8_t keyId)
{
    CHash256 hasher;
    static const char tag[] = "xcoin-xpf-v1";
    hasher.Write(reinterpret_cast<const unsigned char*>(tag), sizeof(tag) - 1);
    const unsigned char hlen = static_cast<unsigned char>(handle.size() > 255 ? 255 : handle.size());
    const size_t hwrite = handle.size() > 255 ? 255 : handle.size();
    hasher.Write(&hlen, 1);
    if (hwrite)
        hasher.Write(reinterpret_cast<const unsigned char*>(handle.data()), hwrite);
    unsigned char uid[8];
    WriteLE64(uid, userId);
    hasher.Write(uid, 8);
    hasher.Write(addrHash.begin(), addrHash.size());
    const unsigned char rlen = static_cast<unsigned char>(root.size() > 255 ? 255 : root.size());
    const size_t rwrite = root.size() > 255 ? 255 : root.size();
    hasher.Write(&rlen, 1);
    if (rwrite)
        hasher.Write(reinterpret_cast<const unsigned char*>(root.data()), rwrite);
    unsigned char exp[4];
    WriteLE32(exp, expiry);
    hasher.Write(exp, 4);
    hasher.Write(&keyId, 1);
    uint256 out;
    hasher.Finalize(out.begin());
    return out;
}

static bool XClaimBindFields(const CTransaction& tx, std::string& handle, uint160& addr, std::string& root)
{
    handle.clear();
    root.clear();
    addr.SetNull();
    if (!IsXAccountIdentityClaim(tx, &handle) || handle.empty() || handle.size() > 255)
        return false;

    bool sawRoot = false;
    bool sawOwner = false;
    uint160 rootId;
    uint160 ownerId;
    for (const auto& out : tx.vout) {
        int nType = 0;
        bool fOwner = false;
        if (!out.scriptPubKey.IsAssetScript(nType, fOwner))
            continue;
        if (nType != TX_NEW_ASSET)
            continue;
        CTxDestination dest;
        if (!ExtractDestination(out.scriptPubKey, dest))
            return false;
        const CKeyID* id = boost::get<CKeyID>(&dest);
        if (!id)
            return false;
        if (fOwner) {
            if (sawOwner)
                return false;
            sawOwner = true;
            ownerId = *id;
        } else {
            if (sawRoot)
                return false;
            CNewAsset asset;
            std::string addrStr;
            if (!AssetFromScript(out.scriptPubKey, asset, addrStr))
                return false;
            if (asset.strName.empty() || asset.strName.size() > 255)
                return false;
            sawRoot = true;
            root = asset.strName;
            rootId = *id;
        }
    }
    if (!sawRoot || !sawOwner || rootId != ownerId)
        return false;
    addr = rootId;
    return true;
}

bool CheckXClaimProof(const CTransaction& tx, int64_t now, std::string& err)
{
    err.clear();
    XClaimProof proof;
    if (!ParseXClaimProof(tx, proof)) {
        err = "xclaim-proof-missing";
        return false;
    }
    const int64_t exp = static_cast<int64_t>(proof.expiry);
    if (now < 0 || exp <= now || exp - now > XCLAIM_PROOF_MAX_FUTURE) {
        err = "xclaim-proof-expired";
        return false;
    }
    std::string handle;
    std::string root;
    uint160 addr;
    if (!XClaimBindFields(tx, handle, addr, root)) {
        err = "xclaim-proof-mismatch";
        return false;
    }
    auto it = g_xclaimProofPubs.find(proof.keyId);
    if (it == g_xclaimProofPubs.end() || !it->second.IsFullyValid() || proof.sig.size() != 65) {
        err = "xclaim-proof-bad-sig";
        return false;
    }
    const uint256 digest = XClaimProofDigest(handle, proof.userId, addr, root, proof.expiry, proof.keyId);
    CPubKey rec;
    if (!rec.RecoverCompact(digest, proof.sig) || rec != it->second) {
        err = "xclaim-proof-bad-sig";
        return false;
    }
    return true;
}

bool XClaimProofEnforced()
{
    if (g_xclaimProofPubs.empty())
        return false;
    for (const auto& kv : g_xclaimProofPubs) {
        if (!kv.second.IsFullyValid())
            return false;
    }
    return true;
}

const std::map<uint8_t, CPubKey>& XClaimProofPubs()
{
    return g_xclaimProofPubs;
}

static bool ParseOneXClaimPub(const std::string& specIn, uint8_t& idOut, CPubKey& pubOut, std::string& err)
{
    std::string spec = specIn;
    spec.erase(std::remove_if(spec.begin(), spec.end(), [](unsigned char c) { return std::isspace(c); }), spec.end());
    if (spec.empty()) {
        err = "empty xclaimproofpubkey entry";
        return false;
    }
    const size_t colon = spec.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= spec.size()) {
        err = "xclaimproofpubkey entries are <id>:<66 hex chars>";
        return false;
    }
    const std::string idStr = spec.substr(0, colon);
    const std::string hex = spec.substr(colon + 1);
    if (idStr.empty() || idStr.size() > 3) {
        err = "xclaimproofpubkey key id must be 0-255";
        return false;
    }
    int id = 0;
    for (char c : idStr) {
        if (c < '0' || c > '9') {
            err = "xclaimproofpubkey key id must be 0-255";
            return false;
        }
        id = id * 10 + (c - '0');
    }
    if (id > 255) {
        err = "xclaimproofpubkey key id must be 0-255";
        return false;
    }
    if (hex.size() != 66 || !IsHex(hex)) {
        err = "xclaimproofpubkey must be a 33-byte compressed pubkey (66 hex chars)";
        return false;
    }
    const std::vector<unsigned char> data = ParseHex(hex);
    if (data.size() != 33) {
        err = "xclaimproofpubkey must be a 33-byte compressed pubkey (66 hex chars)";
        return false;
    }
    CPubKey pub(data.begin(), data.end());
    // Format only. Curve check runs after ECC_Start (AppInitSanityChecks).
    if (!pub.IsValid() || !pub.IsCompressed()) {
        err = "xclaimproofpubkey is not a compressed secp256k1 pubkey";
        return false;
    }
    idOut = static_cast<uint8_t>(id);
    pubOut = pub;
    return true;
}

bool SetXClaimProofPubkeys(const std::vector<std::string>& specs, std::string& err)
{
    err.clear();
    std::map<uint8_t, CPubKey> next;
    for (const std::string& raw : specs) {
        std::string cur;
        for (size_t i = 0; i <= raw.size(); ++i) {
            const bool end = (i == raw.size());
            if (!end && raw[i] != ',') {
                cur.push_back(raw[i]);
                continue;
            }
            std::string piece = cur;
            cur.clear();
            piece.erase(std::remove_if(piece.begin(), piece.end(), [](unsigned char c) { return std::isspace(c); }), piece.end());
            if (piece.empty())
                continue;
            uint8_t id = 0;
            CPubKey pub;
            if (!ParseOneXClaimPub(piece, id, pub, err))
                return false;
            if (next.count(id)) {
                err = "xclaimproofpubkey key id is duplicated";
                return false;
            }
            next[id] = pub;
        }
    }
    g_xclaimProofPubs.swap(next);
    return true;
}

bool XClaimProofPubReady(std::string& err)
{
    err.clear();
    for (const auto& kv : g_xclaimProofPubs) {
        if (!kv.second.IsFullyValid()) {
            err = "xclaimproofpubkey is not on the secp256k1 curve";
            g_xclaimProofPubs.clear();
            return false;
        }
    }
    return true;
}

void SetXClaimProofPubsForTest(const std::map<uint8_t, CPubKey>& pubs)
{
    g_xclaimProofPubs = pubs;
}

bool XClaimProofBlocksInclusion(const CTransaction& tx)
{
    if (!XClaimProofEnforced() || !IsXAccountIdentityClaim(tx))
        return false;
    std::string err;
    return !CheckXClaimProof(tx, GetTime(), err);
}

bool XClaimDatacarrierExempt(const CTransaction& tx, const CScript& scriptPubKey)
{
    if (!fAcceptDatacarrier)
        return false;
    if (scriptPubKey.size() <= nMaxDatacarrierBytes)
        return false;
    if (scriptPubKey.size() > MAX_XCLAIM_DATACARRIER_BYTES)
        return false;
    if (scriptPubKey.empty() || scriptPubKey[0] != OP_RETURN)
        return false;
    if (!IsXAccountIdentityClaim(tx))
        return false;
    XClaimProof proof;
    return ParseXClaimProof(tx, proof);
}

const char* XClaimSignerUnreachableMessage()
{
    return "The name signer is unreachable, so new claims are paused. Coins, Launch, and existing names are unaffected.";
}

const char* XClaimSignInAgainMessage()
{
    return "Sign in with X again to claim a name. The access token is kept in memory for this session only.";
}

std::string FormatXClaimWalletError(const std::string& raw)
{
    if (raw.find("xclaim-proof") == std::string::npos)
        return raw;
    return std::string(XClaimSignerUnreachableMessage()) + " (" + raw + ")";
}

static std::vector<std::string> SplitClaimUrls(const std::string& raw)
{
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i <= raw.size(); ++i) {
        const bool end = (i == raw.size());
        if (!end && raw[i] != ',') {
            cur.push_back(raw[i]);
            continue;
        }
        size_t a = 0;
        while (a < cur.size() && std::isspace(static_cast<unsigned char>(cur[a])))
            a++;
        size_t b = cur.size();
        while (b > a && std::isspace(static_cast<unsigned char>(cur[b - 1])))
            b--;
        if (b > a)
            out.push_back(cur.substr(a, b - a));
        cur.clear();
    }
    return out;
}

std::vector<std::string> XClaimProofUrls()
{
    if (gArgs.IsArgSet("-xclaimproofurls")) {
        std::vector<std::string> out;
        for (const std::string& raw : gArgs.GetArgs("-xclaimproofurls")) {
            const std::vector<std::string> parts = SplitClaimUrls(raw);
            out.insert(out.end(), parts.begin(), parts.end());
        }
        return out;
    }
    if (GetParams().NetworkIDString() == CBaseChainParams::MAIN)
        return {XCLAIM_URL_PRIMARY, XCLAIM_URL_FALLBACK};
    return {};
}

bool XClaimProofShouldAttach()
{
    return XClaimProofEnforced() || !XClaimProofUrls().empty();
}

void SetXClaimHttpPostForTest(XClaimHttpPostFn fn)
{
    g_xclaimHttpPost = fn;
}

static bool JsonAtomOk(const std::string& s)
{
    for (unsigned char c : s) {
        if (c < 0x20 || c == '"' || c == '\\')
            return false;
    }
    return true;
}

static bool ReadJsonU64(const UniValue& v, uint64_t& out)
{
    if (v.isNum()) {
        if (v.get_int64() < 0)
            return false;
        out = static_cast<uint64_t>(v.get_int64());
        return true;
    }
    if (!v.isStr())
        return false;
    const std::string s = v.get_str();
    if (s.empty() || s.size() > 20)
        return false;
    uint64_t n = 0;
    for (char c : s) {
        if (c < '0' || c > '9')
            return false;
        n = n * 10 + static_cast<uint64_t>(c - '0');
    }
    out = n;
    return true;
}

bool XClaimProofFromSignerJson(const std::string& json, const std::string& handle, uint64_t userId, const std::string& address, const std::string& root, int64_t now, std::vector<unsigned char>& payload, std::string& err)
{
    payload.clear();
    err.clear();
    UniValue v;
    if (!v.read(json) || !v.isObject()) {
        err = XClaimSignerUnreachableMessage();
        return false;
    }
    if (v.exists("ok") && !v["ok"].isTrue() && !(v["ok"].isBool() && v["ok"].get_bool())) {
        const std::string why = (v.exists("error") && v["error"].isStr()) ? v["error"].get_str() : "";
        if (why == "sign-in-again")
            err = XClaimSignInAgainMessage();
        else
            err = XClaimSignerUnreachableMessage();
        return false;
    }
    if (!v.exists("keyId") || !v.exists("userId") || !v.exists("expiry") || !v.exists("sig") ||
        !v.exists("handle") || !v.exists("root") || !v.exists("address")) {
        err = XClaimSignerUnreachableMessage();
        return false;
    }
    uint64_t keyId64 = 0;
    uint64_t echoedUid = 0;
    uint64_t expiry64 = 0;
    if (!ReadJsonU64(v["keyId"], keyId64) || keyId64 > 255 ||
        !ReadJsonU64(v["userId"], echoedUid) ||
        !ReadJsonU64(v["expiry"], expiry64) || expiry64 > 0xffffffffu) {
        err = XClaimSignerUnreachableMessage();
        return false;
    }
    if (!v["handle"].isStr() || !v["root"].isStr() || !v["address"].isStr() || !v["sig"].isStr()) {
        err = XClaimSignerUnreachableMessage();
        return false;
    }
    if (v["handle"].get_str() != handle || echoedUid != userId || v["root"].get_str() != root || v["address"].get_str() != address) {
        err = "xclaim-proof-mismatch";
        return false;
    }
    const int64_t exp = static_cast<int64_t>(expiry64);
    if (now < 0 || exp <= now || exp - now > XCLAIM_PROOF_MAX_FUTURE) {
        err = "xclaim-proof-expired";
        return false;
    }
    const std::string sigHex = v["sig"].get_str();
    if (sigHex.size() != 130 || !IsHex(sigHex)) {
        err = "xclaim-proof-bad-sig";
        return false;
    }
    const std::vector<unsigned char> sig = ParseHex(sigHex);
    if (!BuildXClaimProofPayload(static_cast<uint8_t>(keyId64), userId, static_cast<uint32_t>(expiry64), sig, payload)) {
        err = "xclaim-proof-bad-sig";
        return false;
    }
    return true;
}

static void ApplySocketTimeout(BIO* bio)
{
    int fd = -1;
    if (!bio || BIO_get_fd(bio, &fd) <= 0 || fd < 0)
        return;
#ifdef WIN32
    DWORD ms = 8000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
    struct timeval tv;
    tv.tv_sec = 8;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

static bool ReadHttpBody(BIO* bio, int& httpStatus, std::string& body, std::string& err)
{
    httpStatus = 0;
    body.clear();
    std::string raw;
    char buf[2048];
    for (;;) {
        const int n = BIO_read(bio, buf, sizeof(buf));
        if (n > 0)
            raw.append(buf, buf + n);
        else
            break;
    }
    const size_t hdrEnd = raw.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) {
        err = "bad http";
        return false;
    }
    const std::string headers = raw.substr(0, hdrEnd);
    body = raw.substr(hdrEnd + 4);
    if (headers.compare(0, 5, "HTTP/") == 0) {
        const size_t sp = headers.find(' ');
        if (sp != std::string::npos)
            httpStatus = atoi(headers.c_str() + sp + 1);
    }
    std::string low = headers;
    for (char& c : low) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    if (low.find("transfer-encoding: chunked") != std::string::npos) {
        std::string decoded;
        size_t i = 0;
        bool ok = true;
        while (i < body.size()) {
            const size_t nl = body.find("\r\n", i);
            if (nl == std::string::npos) {
                ok = false;
                break;
            }
            const unsigned long sz = strtoul(body.c_str() + i, nullptr, 16);
            i = nl + 2;
            if (sz == 0)
                break;
            if (i + sz > body.size()) {
                ok = false;
                break;
            }
            decoded.append(body, i, sz);
            i += sz;
            if (i + 1 < body.size() && body[i] == '\r' && body[i + 1] == '\n')
                i += 2;
        }
        if (ok)
            body.swap(decoded);
    }
    const size_t start = body.find('{');
    const size_t end = body.rfind('}');
    if (start != std::string::npos && end != std::string::npos && end >= start)
        body = body.substr(start, end - start + 1);
    return true;
}

static bool DefaultXClaimHttpPost(const std::string& url, const std::string& bearer, const std::string& jsonBody, int& httpStatus, std::string& response, std::string& err)
{
    httpStatus = 0;
    response.clear();
    const std::string https = "https://";
    const std::string http = "http://";
    bool tls = false;
    std::string rest;
    std::string port;
    if (url.compare(0, https.size(), https) == 0) {
        tls = true;
        rest = url.substr(https.size());
        port = "443";
    } else if (url.compare(0, http.size(), http) == 0) {
        rest = url.substr(http.size());
        port = "80";
    } else {
        err = "proof url must be http or https";
        return false;
    }
    if (rest.empty() || rest.find('@') != std::string::npos) {
        err = "bad proof url";
        return false;
    }
    const size_t slash = rest.find('/');
    std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    const std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
    if (hostport.empty()) {
        err = "bad proof url";
        return false;
    }
    std::string host = hostport;
    const size_t colon = hostport.rfind(':');
    if (colon != std::string::npos && hostport.find(':') == colon) {
        host = hostport.substr(0, colon);
        port = hostport.substr(colon + 1);
    }
    if (host.empty() || port.empty() || path.empty() || path[0] != '/') {
        err = "bad proof url";
        return false;
    }

    SSL_CTX* ctx = nullptr;
    BIO* bio = nullptr;
    if (tls) {
        SSL_load_error_strings();
        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            err = "ssl ctx";
            return false;
        }
        SSL_CTX_set_timeout(ctx, 8);
        SSL_CTX_set_default_verify_paths(ctx);
        bio = BIO_new_ssl_connect(ctx);
        if (!bio) {
            SSL_CTX_free(ctx);
            err = "ssl bio";
            return false;
        }
        SSL* ssl = nullptr;
        BIO_get_ssl(bio, &ssl);
        if (ssl)
            SSL_set_tlsext_host_name(ssl, host.c_str());
        BIO_set_conn_hostname(bio, (host + ":" + port).c_str());
    } else {
        bio = BIO_new_connect((host + ":" + port).c_str());
        if (!bio) {
            err = "connect bio";
            return false;
        }
    }
    ApplySocketTimeout(bio);
    if (BIO_do_connect(bio) <= 0) {
        err = "connect failed";
        BIO_free_all(bio);
        if (ctx)
            SSL_CTX_free(ctx);
        return false;
    }
    ApplySocketTimeout(bio);
    std::string req = "POST " + path + " HTTP/1.0\r\nHost: " + host + "\r\n";
    if (!bearer.empty())
        req += "Authorization: Bearer " + bearer + "\r\n";
    req += "User-Agent: xcoin-xclaim/1.0\r\nContent-Type: application/json\r\nAccept: application/json\r\nConnection: close\r\nContent-Length: " + std::to_string(jsonBody.size()) + "\r\n\r\n" + jsonBody;
    if (BIO_write(bio, req.data(), (int)req.size()) <= 0) {
        err = "write failed";
        BIO_free_all(bio);
        if (ctx)
            SSL_CTX_free(ctx);
        return false;
    }
    const bool readOk = ReadHttpBody(bio, httpStatus, response, err);
    BIO_free_all(bio);
    if (ctx)
        SSL_CTX_free(ctx);
    return readOk;
}

bool FetchXClaimProof(const std::string& handle, uint64_t userId, const std::string& address, const std::string& root, std::vector<unsigned char>& payload, std::string& err)
{
    payload.clear();
    err.clear();
    const std::vector<std::string> urls = XClaimProofUrls();
    if (urls.empty()) {
        err = XClaimSignerUnreachableMessage();
        return false;
    }
    const std::string bearer = xsession::ClaimBearer();
    if (bearer.empty()) {
        err = XClaimSignInAgainMessage();
        return false;
    }
    if (!JsonAtomOk(handle) || !JsonAtomOk(address) || !JsonAtomOk(root)) {
        err = "xclaim-proof-mismatch";
        return false;
    }
    const std::string body = std::string("{\"handle\":\"") + handle + "\",\"userId\":" + std::to_string(userId) + ",\"address\":\"" + address + "\",\"root\":\"" + root + "\"}";
    bool sawSignIn = false;
    const int64_t now = GetTime();
    for (const std::string& url : urls) {
        int httpStatus = 0;
        std::string response;
        std::string postErr;
        const XClaimHttpPostFn post = g_xclaimHttpPost ? g_xclaimHttpPost : DefaultXClaimHttpPost;
        if (!post(url, bearer, body, httpStatus, response, postErr))
            continue;
        if (httpStatus == 401) {
            sawSignIn = true;
            continue;
        }
        std::string parseErr;
        if (XClaimProofFromSignerJson(response, handle, userId, address, root, now, payload, parseErr)) {
            err.clear();
            return true;
        }
        if (parseErr == XClaimSignInAgainMessage())
            sawSignIn = true;
    }
    payload.clear();
    err = sawSignIn ? XClaimSignInAgainMessage() : XClaimSignerUnreachableMessage();
    return false;
}

std::string TopLevelRootName(const std::string& assetName)
{
    std::string root = assetName;
    for (int i = 0; i < 32; i++) {
        const std::string parent = GetParentName(root);
        if (parent.empty() || parent == root)
            break;
        root = parent;
    }
    return root;
}

bool RequireIssueUnderOwnMain(const std::string& assetName, std::string& err)
{
    err.clear();
    if (!xsession::RequireSession(err))
        return false;
    const std::string handle = xsession::SignedInHandle();
    std::string ownRoot;
    if (!CheckIfXAccountAssigned(handle, &ownRoot) || ownRoot.empty()) {
        err = "Claim your main asset first (Sign in with X, then linkxaccount). Subs and uniques are issued under that root.";
        return false;
    }
    const std::string root = TopLevelRootName(assetName);
    if (root.empty() || root != ownRoot) {
        err = "Subs and uniques can only be issued under your signed-in main asset (" + ownRoot + ")";
        return false;
    }
    return true;
}

bool CheckIfXAccountAssigned(const std::string& xId, std::string* assetName)
{
    {
        LOCK(cs_xaccount);
        auto it = mapXAccountToAsset.find(xId);
        if (it != mapXAccountToAsset.end()) {
            if (assetName)
                *assetName = it->second;
            return true;
        }
    }
    {
        LOCK(mempool.cs);
        auto it = mempool.mapXAccountToHash.find(xId);
        if (it != mempool.mapXAccountToHash.end()) {
            if (assetName) {
                auto nameIt = mempool.mapHashToAsset.find(it->second);
                if (nameIt != mempool.mapHashToAsset.end())
                    *assetName = nameIt->second;
            }
            return true;
        }
    }
    return false;
}

bool AddXAccountAssignment(const std::string& xId, const std::string& assetName)
{
    {
        LOCK(cs_xaccount);
        auto it = mapXAccountToAsset.find(xId);
        if (it != mapXAccountToAsset.end() && it->second != assetName)
            return error("%s: X account %s already assigned to %s", __func__, xId, it->second);
        mapXAccountToAsset[xId] = assetName;
        mapAssetToXAccount[assetName] = xId;
    }
    if (passetsdb)
        passetsdb->WriteXAccountAssignment(xId, assetName);
    return true;
}

bool RemoveXAccountAssignment(const std::string& xId)
{
    std::string asset;
    {
        LOCK(cs_xaccount);
        auto it = mapXAccountToAsset.find(xId);
        if (it == mapXAccountToAsset.end())
            return true;
        asset = it->second;
        mapXAccountToAsset.erase(it);
        mapAssetToXAccount.erase(asset);
    }
    if (passetsdb)
        passetsdb->EraseXAccountAssignment(xId);
    return true;
}

bool LoadXAccountAssignments()
{
    if (!passetsdb)
        return true;
    std::map<std::string, std::string> loaded;
    if (!passetsdb->LoadXAccountAssignments(loaded))
        return false;
    LOCK(cs_xaccount);
    mapXAccountToAsset = loaded;
    mapAssetToXAccount.clear();
    for (const auto& kv : loaded)
        mapAssetToXAccount[kv.second] = kv.first;
    LogPrintf("assets: loaded %u X-account main-asset assignment(s)\n",
              (unsigned)mapXAccountToAsset.size());
    return true;
}

#ifdef ENABLE_WALLET
bool AssignLinkedUserMainAsset(const std::string& xHandleOrId, const std::string& destIn,
                               std::string& err, std::string* assetNameOut, std::string* txidOut)
{
    err.clear();
    if (assetNameOut)
        assetNameOut->clear();
    if (txidOut)
        txidOut->clear();

    std::string handle;
    uint64_t userId = 0;
    if (!NormalizeXAccountId(xHandleOrId, handle, userId, err))
        return false;
    if (!xsession::RequireHandle(handle, err))
        return false;

    std::string existing;
    if (CheckIfXAccountAssigned(handle, &existing)) {
        if (assetNameOut)
            *assetNameOut = existing;
        return true;
    }

    if (vpwallets.empty() || !vpwallets[0]) {
        err = "Wallet not available";
        return false;
    }
    CWallet* pwallet = vpwallets[0];
    LOCK2(cs_main, pwallet->cs_wallet);
    if (pwallet->IsLocked()) {
        err = "Wallet is locked";
        return false;
    }

    std::string name;
    if (!DeriveMainAssetName(userId ? (handle + ":" + std::to_string(userId)) : handle, name, err, GetCurrentAssetCache()))
        return false;

    if (CheckIfXAccountAssigned(handle, &existing)) {
        if (assetNameOut)
            *assetNameOut = existing;
        return true;
    }

    std::string dest = destIn;
    if (dest.empty()) {
        if (!pwallet->IsLocked())
            pwallet->TopUpKeyPool();
        CPubKey newKey;
        if (!pwallet->GetKeyFromPool(newKey)) {
            err = "Error: Keypool ran out, please call keypoolrefill first";
            return false;
        }
        CKeyID keyID = newKey.GetID();
        pwallet->SetAddressBook(keyID, "", "receive");
        dest = EncodeDestination(keyID);
    } else {
        CTxDestination destination = DecodeDestination(dest);
        if (!IsValidDestination(destination)) {
            err = "Invalid destination address";
            return false;
        }
    }

    uint64_t claimUid = userId;
    if (XClaimProofShouldAttach()) {
        const std::string sid = xsession::SignedInUserId();
        if (sid.empty()) {
            err = XClaimSignInAgainMessage();
            return false;
        }
        uint64_t sessionUid = 0;
        for (char c : sid) {
            if (c < '0' || c > '9') {
                err = XClaimSignInAgainMessage();
                return false;
            }
            sessionUid = sessionUid * 10 + static_cast<uint64_t>(c - '0');
        }
        if (userId != 0 && userId != sessionUid) {
            err = "xclaim-proof-mismatch";
            return false;
        }
        claimUid = sessionUid;
    }

    std::vector<unsigned char> proofPayload;
    const std::vector<unsigned char>* proofPtr = nullptr;
    if (XClaimProofShouldAttach()) {
        if (!FetchXClaimProof(handle, claimUid, dest, name, proofPayload, err))
            return false;
        proofPtr = &proofPayload;
    }

    CNewAsset asset(name, MAIN_ASSET_CIRCULATING_AMOUNT, 0, 0, 0, "");

    CReserveKey reservekey(pwallet);
    CWalletTx transaction;
    CAmount nRequiredFee;
    std::pair<int, std::string> error;
    CCoinControl ctrl;
    if (!CreateAssetTransaction(pwallet, ctrl, asset, dest, error, transaction, reservekey, nRequiredFee, nullptr, &handle, proofPtr)) {
        err = error.second;
        return false;
    }

    std::string txid;
    if (!SendAssetTransaction(pwallet, transaction, reservekey, error, txid)) {
        err = FormatXClaimWalletError(error.second);
        return false;
    }

    if (assetNameOut)
        *assetNameOut = name;
    if (txidOut)
        *txidOut = txid;
    LogPrintf("assets: assigned main asset %s to X account %s (txid %s)\n", name, handle, txid);
    return true;
}
#endif
