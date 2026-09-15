// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xmarket.h"

#include "assets/assets.h"
#include "coins.h"
#include "net.h"
#include "netmessagemaker.h"
#include "policy/policy.h"
#include "protocol.h"
#include "script/interpreter.h"
#include "script/script.h"
#include "serialize.h"
#include "streams.h"
#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "validation.h"
#include "version.h"

#include <deque>
#include <map>

namespace xmarket {

static CCriticalSection cs_asks;
static std::map<uint256, CTransactionRef> mapAsks;
static std::map<uint256, int64_t> mapAskTime;

static bool ScriptSigIsSingleAnyoneCanPay(const CScript& scriptSig)
{
    CScript::const_iterator pc = scriptSig.begin();
    std::vector<unsigned char> vch;
    opcodetype opcode;
    if (!scriptSig.GetOp(pc, opcode, vch) || vch.empty())
        return false;
    const unsigned char nHashType = vch.back();
    return ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE) &&
           (nHashType & SIGHASH_ANYONECANPAY);
}

static bool RateOk(NodeId id)
{
    static CCriticalSection cs_rate;
    static std::map<NodeId, std::deque<int64_t> > hits;
    const int64_t now = GetTime();
    LOCK(cs_rate);
    std::deque<int64_t>& q = hits[id];
    while (!q.empty() && now - q.front() > 60)
        q.pop_front();
    if (q.size() >= 30)
        return false;
    q.push_back(now);
    return true;
}

static void DropOldestLocked()
{
    AssertLockHeld(cs_asks);
    if (mapAsks.empty())
        return;
    uint256 oldest;
    int64_t oldestTime = 0;
    bool have = false;
    for (std::map<uint256, int64_t>::const_iterator it = mapAskTime.begin(); it != mapAskTime.end(); ++it) {
        if (!have || it->second < oldestTime) {
            oldestTime = it->second;
            oldest = it->first;
            have = true;
        }
    }
    if (have) {
        mapAsks.erase(oldest);
        mapAskTime.erase(oldest);
    }
}

void PruneSpent()
{
    if (!pcoinsTip)
        return;
    std::vector<uint256> dead;
    std::vector<std::pair<uint256, COutPoint> > check;
    {
        LOCK(cs_asks);
        for (std::map<uint256, CTransactionRef>::const_iterator it = mapAsks.begin(); it != mapAsks.end(); ++it) {
            if (!it->second || it->second->vin.empty())
                dead.push_back(it->first);
            else
                check.push_back(std::make_pair(it->first, it->second->vin[0].prevout));
        }
    }
    {
        LOCK(cs_main);
        for (size_t i = 0; i < check.size(); ++i) {
            const Coin& coin = pcoinsTip->AccessCoin(check[i].second);
            if (coin.IsSpent())
                dead.push_back(check[i].first);
        }
    }
    if (dead.empty())
        return;
    LOCK(cs_asks);
    for (size_t i = 0; i < dead.size(); ++i) {
        mapAsks.erase(dead[i]);
        mapAskTime.erase(dead[i]);
    }
}

bool ValidateAsk(const CTransaction& tx, std::string& strAsset, CAmount& nAmount, CAmount& nPrice, std::string& err)
{
    strAsset.clear();
    nAmount = 0;
    nPrice = 0;
    if (tx.vin.size() != 1 || tx.vout.size() != 1) {
        err = "Listing must spend one asset output and pay XFER in a single first output.";
        return false;
    }
    const unsigned int nBytes = GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
    if (nBytes > MAX_ASK_BYTES) {
        err = "Listing is too large.";
        return false;
    }
    const CTxOut& pay = tx.vout[0];
    if (pay.nValue <= 0) {
        err = "Price must be XFER in the first output.";
        return false;
    }
    std::string dummyName;
    CAmount dummyAmt = 0;
    if (GetAssetInfoFromScript(pay.scriptPubKey, dummyName, dummyAmt)) {
        err = "Price output cannot be an asset.";
        return false;
    }
    if (tx.vin[0].scriptSig.empty() || !ScriptSigIsSingleAnyoneCanPay(tx.vin[0].scriptSig)) {
        err = "Listing must be signed SINGLE|ANYONECANPAY so a buyer can complete it.";
        return false;
    }
    if (!pcoinsTip) {
        err = "Chain is not ready.";
        return false;
    }
    Coin coin;
    {
        LOCK(cs_main);
        coin = pcoinsTip->AccessCoin(tx.vin[0].prevout);
    }
    if (coin.IsSpent()) {
        err = "That asset output is already spent.";
        return false;
    }
    if (!GetAssetInfoFromScript(coin.out.scriptPubKey, strAsset, nAmount) || strAsset.empty() || nAmount <= 0) {
        err = "First input must be an asset.";
        return false;
    }
    if (strAsset.find('~') != std::string::npos || (!strAsset.empty() && strAsset[strAsset.size() - 1] == '!')) {
        err = "Owner tokens cannot be listed.";
        return false;
    }
    ScriptError serror = SCRIPT_ERR_OK;
    const TransactionSignatureChecker checker(&tx, 0, coin.out.nValue);
    if (!VerifyScript(tx.vin[0].scriptSig, coin.out.scriptPubKey, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, checker, &serror)) {
        err = "Listing signature is not valid.";
        return false;
    }
    nPrice = pay.nValue;
    return true;
}

bool AcceptAsk(const CTransaction& tx, std::string& err)
{
    PruneSpent();
    std::string strAsset;
    CAmount nAmount = 0;
    CAmount nPrice = 0;
    if (!ValidateAsk(tx, strAsset, nAmount, nPrice, err))
        return false;

    const uint256 hash = tx.GetHash();
    const COutPoint listed = tx.vin[0].prevout;
    CTransactionRef ptx = MakeTransactionRef(tx);

    LOCK(cs_asks);
    std::vector<uint256> replace;
    for (std::map<uint256, CTransactionRef>::const_iterator it = mapAsks.begin(); it != mapAsks.end(); ++it) {
        if (it->second && !it->second->vin.empty() && it->second->vin[0].prevout == listed)
            replace.push_back(it->first);
    }
    for (size_t i = 0; i < replace.size(); ++i) {
        mapAsks.erase(replace[i]);
        mapAskTime.erase(replace[i]);
    }
    if (mapAsks.count(hash))
        return true;
    while (mapAsks.size() >= MAX_ASKS)
        DropOldestLocked();
    mapAsks[hash] = ptx;
    mapAskTime[hash] = GetTime();
    LogPrint(BCLog::NET, "xord listed %s amount %d price %d id=%s\n",
             strAsset, nAmount, nPrice, hash.GetHex());
    return true;
}

void RemoveAsk(const uint256& hash)
{
    LOCK(cs_asks);
    mapAsks.erase(hash);
    mapAskTime.erase(hash);
}

CTransactionRef GetAsk(const uint256& hash)
{
    LOCK(cs_asks);
    std::map<uint256, CTransactionRef>::const_iterator it = mapAsks.find(hash);
    if (it == mapAsks.end())
        return CTransactionRef();
    return it->second;
}

std::vector<CTransactionRef> ListAsks()
{
    PruneSpent();
    LOCK(cs_asks);
    std::vector<CTransactionRef> out;
    out.reserve(mapAsks.size());
    for (std::map<uint256, CTransactionRef>::const_iterator it = mapAsks.begin(); it != mapAsks.end(); ++it) {
        if (it->second)
            out.push_back(it->second);
    }
    return out;
}

void RelayAsk(const CTransaction& tx, CConnman& connman, CNode* pfrom)
{
    connman.ForEachNode([&](CNode* pto) {
        if (pto == pfrom || !pto->fSuccessfullyConnected || pto->fDisconnect)
            return;
        CNetMsgMaker relayMaker(pto->GetSendVersion());
        connman.PushMessage(pto, relayMaker.Make(NetMsgType::XORD, tx));
    });
}

void PushBook(CNode* pto, CConnman& connman)
{
    if (!pto || !pto->fSuccessfullyConnected || pto->fDisconnect)
        return;
    const std::vector<CTransactionRef> asks = ListAsks();
    CNetMsgMaker msgMaker(pto->GetSendVersion());
    size_t n = 0;
    for (size_t i = 0; i < asks.size(); ++i) {
        if (!asks[i])
            continue;
        if (++n > MAX_PUSH_ON_CONNECT)
            break;
        connman.PushMessage(pto, msgMaker.Make(NetMsgType::XORD, *asks[i]));
    }
}

int ProcessXord(CNode* pfrom, CDataStream& vRecv, CConnman& connman)
{
    if (!pfrom)
        return 0;
    if (vRecv.size() > MAX_ASK_BYTES + 32)
        return 20;
    if (!RateOk(pfrom->GetId()))
        return 1;

    CTransactionRef ptx;
    try {
        vRecv >> ptx;
    } catch (const std::exception&) {
        return 1;
    }
    if (!ptx)
        return 1;
    if (GetSerializeSize(*ptx, SER_NETWORK, PROTOCOL_VERSION) > MAX_ASK_BYTES)
        return 20;

    const uint256 hash = ptx->GetHash();
    {
        LOCK(cs_asks);
        if (mapAsks.count(hash))
            return 0;
    }

    std::string err;
    const bool ok = AcceptAsk(*ptx, err);
    if (!ok)
        return 0;
    {
        LOCK(cs_asks);
        if (!mapAsks.count(hash))
            return 0;
    }
    RelayAsk(*ptx, connman, pfrom);
    return 0;
}

} // namespace xmarket
