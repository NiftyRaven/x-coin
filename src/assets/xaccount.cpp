// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assets/xaccount.h"

#include "assets/assetdb.h"
#include "assets/assets.h"
#include "base58.h"
#include "lottery.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "txmempool.h"
#include "util.h"
#include "validation.h"

#ifdef ENABLE_WALLET
#include "rpc/protocol.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"
#endif

#include <cctype>
#include <map>
#include <vector>

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

static std::string UpperHandleToRoot(const std::string& handle)
{
    std::string out;
    out.reserve(handle.size());
    for (unsigned char c : handle) {
        if (c >= 'a' && c <= 'z')
            out.push_back(static_cast<char>(c - 'a' + 'A'));
        else
            out.push_back(static_cast<char>(c));
    }
    while (!out.empty() && (out.front() == '.' || out.front() == '_'))
        out.erase(out.begin());
    while (!out.empty() && (out.back() == '.' || out.back() == '_'))
        out.pop_back();
    std::string collapsed;
    collapsed.reserve(out.size());
    char prev = 0;
    for (char c : out) {
        if ((c == '.' || c == '_') && (prev == '.' || prev == '_'))
            continue;
        collapsed.push_back(c);
        prev = c;
    }
    out.swap(collapsed);
    while (out.size() < MIN_ASSET_LENGTH)
        out.insert(out.begin(), 'X');
    if (out.size() > 30)
        out.resize(30);
    while (!out.empty() && (out.back() == '.' || out.back() == '_'))
        out.pop_back();
    while (out.size() < MIN_ASSET_LENGTH)
        out.push_back('X');
    return out;
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

    std::string base = UpperHandleToRoot(handle);
    auto tryName = [&](const std::string& n) -> bool {
        if (n.size() < MIN_ASSET_LENGTH || n.size() > 30)
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
        std::string n = base;
        if (n.size() + suf.size() > 30) {
            if (suf.size() >= 30)
                continue;
            n.resize(30 - suf.size());
            while (!n.empty() && (n.back() == '.' || n.back() == '_'))
                n.pop_back();
        }
        n += suf;
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

bool ParseXAccountAssignment(const CTransaction& tx, std::string& xId)
{
    for (const auto& out : tx.vout) {
        if (ParseXAccountAssignmentScript(out.scriptPubKey, xId))
            return true;
    }
    return false;
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

    CNewAsset asset(name, MAIN_ASSET_CIRCULATING_AMOUNT, 0, 0, 0, "");

    CReserveKey reservekey(pwallet);
    CWalletTx transaction;
    CAmount nRequiredFee;
    std::pair<int, std::string> error;
    CCoinControl ctrl;
    if (!CreateAssetTransaction(pwallet, ctrl, asset, dest, error, transaction, reservekey, nRequiredFee, nullptr, &handle)) {
        err = error.second;
        return false;
    }

    std::string txid;
    if (!SendAssetTransaction(pwallet, transaction, reservekey, error, txid)) {
        err = error.second;
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
