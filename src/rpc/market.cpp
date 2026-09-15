// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assets/assets.h"
#include "assets/assettypes.h"
#include "amount.h"
#include "base58.h"
#include "coins.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "net.h"
#include "policy/policy.h"
#include "primitives/transaction.h"
#include "protocol.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "script/interpreter.h"
#include "script/ismine.h"
#include "script/sign.h"
#include "script/standard.h"
#include "txmempool.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "validation.h"
#include "xmarket.h"

#ifdef ENABLE_WALLET
#include "wallet/rpcwallet.h"
#include "wallet/wallet.h"
#endif

#include <stdexcept>
#include <univalue.h>

static const CAmount MARKET_FEE = COIN / 100; // 0.01 XFER

static UniValue AskToJSON(const CTransaction& tx, bool fMine)
{
    std::string strAsset;
    CAmount nAmount = 0;
    CAmount nPrice = 0;
    std::string err;
    xmarket::ValidateAsk(tx, strAsset, nAmount, nPrice, err);

    UniValue o(UniValue::VOBJ);
    o.pushKV("id", tx.GetHash().GetHex());
    o.pushKV("asset", strAsset);
    o.pushKV("amount", ValueFromAmount(nAmount));
    o.pushKV("price", ValueFromAmount(nPrice));
    if (!tx.vin.empty()) {
        o.pushKV("asset_txid", tx.vin[0].prevout.hash.GetHex());
        o.pushKV("asset_vout", (int)tx.vin[0].prevout.n);
    }
    o.pushKV("hex", EncodeHexTx(tx));
    o.pushKV("mine", fMine);
    return o;
}

#ifdef ENABLE_WALLET
static bool AskIsMine(CWallet* pwallet, const CTransaction& tx)
{
    if (!pwallet || tx.vin.empty() || !pcoinsTip)
        return false;
    LOCK2(cs_main, pwallet->cs_wallet);
    const Coin& coin = pcoinsTip->AccessCoin(tx.vin[0].prevout);
    if (coin.IsSpent())
        return false;
    return (pwallet->IsMine(coin.out) & ISMINE_SPENDABLE) != 0;
}

static CTxDestination NewMarketDest(CWallet* pwallet, const std::string& label)
{
    CPubKey pubkey;
    if (!pwallet->GetKeyFromPool(pubkey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Unlock the wallet and try again.");
    const CKeyID id = pubkey.GetID();
    pwallet->SetAddressBook(id, label, "receive");
    return id;
}

static bool GetPrevOut(CWallet* pwallet, const COutPoint& op, CTxOut& out)
{
    if (pcoinsTip) {
        LOCK(cs_main);
        const Coin& coin = pcoinsTip->AccessCoin(op);
        if (!coin.IsSpent()) {
            out = coin.out;
            return true;
        }
    }
    LOCK(pwallet->cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator it = pwallet->mapWallet.find(op.hash);
    if (it != pwallet->mapWallet.end() && op.n < it->second.tx->vout.size()) {
        out = it->second.tx->vout[op.n];
        return true;
    }
    return false;
}

static void SignWalletTx(CWallet* pwallet, CMutableTransaction& mtx, int nHashType, unsigned int nStart = 0)
{
    const CTransaction txConst(mtx);
    for (unsigned int i = nStart; i < mtx.vin.size(); ++i) {
        CTxOut prev;
        if (!GetPrevOut(pwallet, mtx.vin[i].prevout, prev))
            throw JSONRPCError(RPC_WALLET_ERROR, "Could not find an input to sign.");
        SignatureData sigdata;
        const bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);
        if (!fHashSingle || i < mtx.vout.size())
            ProduceSignature(MutableTransactionSignatureCreator(pwallet, &mtx, i, prev.nValue, nHashType),
                             prev.scriptPubKey, sigdata);
        sigdata = CombineSignatures(prev.scriptPubKey,
                                    TransactionSignatureChecker(&txConst, i, prev.nValue),
                                    sigdata, DataFromTransaction(mtx, i));
        UpdateTransaction(mtx, i, sigdata);
    }
}

static uint256 BroadcastRaw(CMutableTransaction& mtx)
{
    CTransactionRef tx = MakeTransactionRef(std::move(mtx));
    const uint256 hash = tx->GetHash();
    CValidationState state;
    bool fMissingInputs = false;
    {
        LOCK(cs_main);
        if (!AcceptToMemoryPool(mempool, state, tx, &fMissingInputs, nullptr, false, maxTxFee)) {
            if (state.IsInvalid())
                throw JSONRPCError(RPC_TRANSACTION_REJECTED,
                                   strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason()));
            if (fMissingInputs)
                throw JSONRPCError(RPC_TRANSACTION_ERROR, "Missing inputs");
            throw JSONRPCError(RPC_TRANSACTION_ERROR, state.GetRejectReason());
        }
    }
    if (!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Peer-to-peer is disabled.");
    CInv inv(MSG_TX, hash);
    g_connman->ForEachNode([&inv](CNode* pnode) {
        pnode->PushInventory(inv);
    });
    return hash;
}

static bool PickXferInputs(CWallet* pwallet, const CAmount nNeed, std::vector<COutPoint>& vPicked, CAmount& nValue)
{
    vPicked.clear();
    nValue = 0;
    std::vector<COutput> vCoins;
    std::map<std::string, std::vector<COutput> > mapAssetCoins;
    pwallet->AvailableCoinsWithAssets(vCoins, mapAssetCoins, true, nullptr, 1);
    for (size_t i = 0; i < vCoins.size(); ++i) {
        const COutput& out = vCoins[i];
        if (!out.fSpendable || !out.tx || !out.tx->tx)
            continue;
        if (out.i < 0 || (size_t)out.i >= out.tx->tx->vout.size())
            continue;
        const CTxOut& txout = out.tx->tx->vout[out.i];
        if (txout.nValue <= 0)
            continue;
        std::string name;
        CAmount amt = 0;
        if (GetAssetInfoFromScript(txout.scriptPubKey, name, amt))
            continue;
        vPicked.push_back(COutPoint(out.tx->GetHash(), out.i));
        nValue += txout.nValue;
        if (nValue >= nNeed)
            return true;
    }
    return false;
}

static bool PickAssetOut(CWallet* pwallet, const std::string& strAsset, const uint256* pTxid, int nVout,
                         COutPoint& outpoint, CAmount& nAmount)
{
    std::vector<COutput> vCoins;
    std::map<std::string, std::vector<COutput> > mapAssetCoins;
    pwallet->AvailableCoinsWithAssets(vCoins, mapAssetCoins, true, nullptr, 1);
    std::map<std::string, std::vector<COutput> >::const_iterator it = mapAssetCoins.find(strAsset);
    if (it == mapAssetCoins.end())
        return false;
    const std::vector<COutput>& outs = it->second;
    int best = -1;
    CAmount bestAmt = 0;
    for (size_t i = 0; i < outs.size(); ++i) {
        const COutput& out = outs[i];
        if (!out.fSpendable || !out.tx || !out.tx->tx)
            continue;
        if (out.i < 0 || (size_t)out.i >= out.tx->tx->vout.size())
            continue;
        std::string name;
        CAmount amt = 0;
        if (!GetAssetInfoFromScript(out.tx->tx->vout[out.i].scriptPubKey, name, amt) || name != strAsset || amt <= 0)
            continue;
        const uint256 txid = out.tx->GetHash();
        if (pTxid) {
            if (*pTxid != txid || nVout != out.i)
                continue;
            outpoint = COutPoint(txid, out.i);
            nAmount = amt;
            return true;
        }
        if (best < 0 || amt > bestAmt) {
            best = (int)i;
            bestAmt = amt;
            outpoint = COutPoint(txid, out.i);
            nAmount = amt;
        }
    }
    return best >= 0;
}
#endif

UniValue listmarket(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "listmarket ( asset )\n"
            "\nList asset-for-XFER asks this node has heard over P2P.\n"
            "Listings are partial SINGLE|ANYONECANPAY transactions, not a shop server.\n"
            "\nArguments:\n"
            "1. asset    (string, optional) Only this asset name\n"
        );

    const std::string filter = request.params.size() > 0 && !request.params[0].isNull()
        ? request.params[0].get_str() : "";

    const std::vector<CTransactionRef> asks = xmarket::ListAsks();
    UniValue arr(UniValue::VARR);
#ifdef ENABLE_WALLET
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
#endif
    for (size_t i = 0; i < asks.size(); ++i) {
        if (!asks[i])
            continue;
        std::string strAsset;
        CAmount nAmount = 0;
        CAmount nPrice = 0;
        std::string err;
        if (!xmarket::ValidateAsk(*asks[i], strAsset, nAmount, nPrice, err))
            continue;
        if (!filter.empty() && strAsset != filter)
            continue;
        bool fMine = false;
#ifdef ENABLE_WALLET
        fMine = AskIsMine(pwallet, *asks[i]);
#endif
        arr.push_back(AskToJSON(*asks[i], fMine));
    }
    return arr;
}

#ifdef ENABLE_WALLET
UniValue postask(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 4)
        throw std::runtime_error(
            "postask \"asset\" price ( \"txid\" vout )\n"
            "\nList one asset UTXO for XFER. The whole bag is for sale at that price.\n"
            "This wallet signs SINGLE|ANYONECANPAY and gossips the ask to peers.\n"
            "\nArguments:\n"
            "1. asset    (string, required) Asset name\n"
            "2. price    (numeric or string, required) XFER the buyer pays you\n"
            "3. txid     (string, optional) Specific asset output\n"
            "4. vout     (numeric, optional) Specific asset output index\n"
        );

    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    ObserveSafeMode();
    EnsureWalletIsUnlocked(pwallet);

    if (!AreAssetsDeployed())
        throw JSONRPCError(RPC_MISC_ERROR, "Assets are not active yet.");

    const std::string strAsset = request.params[0].get_str();
    const CAmount nPrice = AmountFromValue(request.params[1]);
    if (nPrice <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Price must be greater than zero.");
    if (strAsset.empty() || strAsset.find('~') != std::string::npos || strAsset[strAsset.size() - 1] == '!')
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot list that asset.");

    uint256 pickTxid;
    const uint256* pTxid = nullptr;
    int nVout = -1;
    if (request.params.size() >= 3 && !request.params[2].isNull() && !request.params[2].get_str().empty()) {
        pickTxid.SetHex(request.params[2].get_str());
        pTxid = &pickTxid;
        if (request.params.size() < 4)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "vout is required with txid.");
        nVout = request.params[3].get_int();
    }

    COutPoint assetOut;
    CAmount nAmount = 0;
    if (!PickAssetOut(pwallet, strAsset, pTxid, nVout, assetOut, nAmount))
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
                           "No spendable output of that asset. Receive it, or send it to yourself first.");

    CTxOut dummyPay(nPrice, GetScriptForDestination(CKeyID()));
    if (IsDust(dummyPay, dustRelayFee))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Price is below the dust threshold.");

    CTxDestination payDest;
    {
        LOCK(pwallet->cs_wallet);
        payDest = NewMarketDest(pwallet, "Market listing");
        pwallet->LockCoin(assetOut);
    }

    CMutableTransaction mtx;
    mtx.nVersion = CTransaction::CURRENT_VERSION;
    mtx.nLockTime = 0;
    mtx.vin.push_back(CTxIn(assetOut, CScript(), CTxIn::SEQUENCE_FINAL));
    mtx.vout.push_back(CTxOut(nPrice, GetScriptForDestination(payDest)));

    SignWalletTx(pwallet, mtx, SIGHASH_SINGLE | SIGHASH_ANYONECANPAY);

    CTransaction signedTx(mtx);
    std::string err;
    if (!xmarket::AcceptAsk(signedTx, err)) {
        LOCK(pwallet->cs_wallet);
        pwallet->UnlockCoin(assetOut);
        throw JSONRPCError(RPC_WALLET_ERROR, err.empty() ? "Could not list that asset." : err);
    }
    if (g_connman)
        xmarket::RelayAsk(signedTx, *g_connman, nullptr);

    return AskToJSON(signedTx, true);
}

UniValue takeask(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "takeask \"id\"\n"
            "\nBuy a listed asset with XFER from this wallet. No copy-paste.\n"
            "Adds your inputs and the asset output, signs, and broadcasts.\n"
            "\nArguments:\n"
            "1. id    (string, required) Listing id from listmarket\n"
        );

    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    ObserveSafeMode();
    EnsureWalletIsUnlocked(pwallet);

    const uint256 id = ParseHashV(request.params[0], "id");
    CTransactionRef pAsk = xmarket::GetAsk(id);
    if (!pAsk) {
        xmarket::PruneSpent();
        pAsk = xmarket::GetAsk(id);
    }
    if (!pAsk)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "That listing is gone.");

    std::string strAsset;
    CAmount nAmount = 0;
    CAmount nPrice = 0;
    std::string err;
    if (!xmarket::ValidateAsk(*pAsk, strAsset, nAmount, nPrice, err))
        throw JSONRPCError(RPC_INVALID_PARAMETER, err.empty() ? "That listing is not valid." : err);

    if (AskIsMine(pwallet, *pAsk))
        throw JSONRPCError(RPC_WALLET_ERROR, "That is your listing. Cancel it instead.");

    const CAmount nNeed = nPrice + MARKET_FEE;
    std::vector<COutPoint> vPay;
    CAmount nPayIn = 0;
    if (!PickXferInputs(pwallet, nNeed, vPay, nPayIn))
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
                           strprintf("Need at least %s XFER plus the 0.01 network fee.",
                                     ValueFromAmount(nPrice).getValStr()));

    CTxDestination assetDest;
    CTxDestination changeDest;
    {
        LOCK(pwallet->cs_wallet);
        assetDest = NewMarketDest(pwallet, "Market buy");
        changeDest = NewMarketDest(pwallet, "Market change");
    }

    CMutableTransaction mtx;
    mtx.nVersion = pAsk->nVersion;
    mtx.nLockTime = pAsk->nLockTime;
    mtx.vin.push_back(pAsk->vin[0]);
    mtx.vout.push_back(pAsk->vout[0]);
    for (size_t i = 0; i < vPay.size(); ++i)
        mtx.vin.push_back(CTxIn(vPay[i], CScript(), CTxIn::SEQUENCE_FINAL));

    const CAmount nChange = nPayIn - nPrice - MARKET_FEE;
    if (nChange > 0) {
        CTxOut changeOut(nChange, GetScriptForDestination(changeDest));
        if (!IsDust(changeOut, dustRelayFee))
            mtx.vout.push_back(changeOut);
    }

    CScript assetScript = GetScriptForDestination(assetDest);
    CAssetTransfer transfer(strAsset, nAmount);
    std::string transferErr;
    if (!transfer.IsValid(transferErr))
        throw JSONRPCError(RPC_WALLET_ERROR, transferErr);
    transfer.ConstructTransaction(assetScript);
    mtx.vout.push_back(CTxOut(0, assetScript));

    SignWalletTx(pwallet, mtx, SIGHASH_ALL, 1);

    const CTransaction checkTx(mtx);
    for (unsigned int i = 0; i < checkTx.vin.size(); ++i) {
        CTxOut prev;
        if (!GetPrevOut(pwallet, checkTx.vin[i].prevout, prev))
            throw JSONRPCError(RPC_WALLET_ERROR, "Could not find an input after signing.");
        ScriptError serror = SCRIPT_ERR_OK;
        if (!VerifyScript(checkTx.vin[i].scriptSig, prev.scriptPubKey, nullptr,
                          STANDARD_SCRIPT_VERIFY_FLAGS,
                          TransactionSignatureChecker(&checkTx, i, prev.nValue), &serror))
            throw JSONRPCError(RPC_WALLET_ERROR, "Could not finish the signatures.");
    }

    const uint256 txid = BroadcastRaw(mtx);
    xmarket::RemoveAsk(id);
    xmarket::PruneSpent();

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("txid", txid.GetHex());
    ret.pushKV("asset", strAsset);
    ret.pushKV("amount", ValueFromAmount(nAmount));
    ret.pushKV("price", ValueFromAmount(nPrice));
    return ret;
}

UniValue cancelask(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "cancelask \"id\"\n"
            "\nTake a listing down by spending the listed asset back to this wallet.\n"
            "\nArguments:\n"
            "1. id    (string, required) Listing id from listmarket\n"
        );

    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    ObserveSafeMode();
    EnsureWalletIsUnlocked(pwallet);

    const uint256 id = ParseHashV(request.params[0], "id");
    CTransactionRef pAsk = xmarket::GetAsk(id);
    if (!pAsk)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "That listing is gone.");
    if (!AskIsMine(pwallet, *pAsk))
        throw JSONRPCError(RPC_WALLET_ERROR, "That listing is not from this wallet.");

    std::string strAsset;
    CAmount nAmount = 0;
    CAmount nPrice = 0;
    std::string err;
    if (!xmarket::ValidateAsk(*pAsk, strAsset, nAmount, nPrice, err))
        throw JSONRPCError(RPC_INVALID_PARAMETER, err.empty() ? "That listing is not valid." : err);

    std::vector<COutPoint> vPay;
    CAmount nPayIn = 0;
    if (!PickXferInputs(pwallet, MARKET_FEE, vPay, nPayIn))
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Need 0.01 XFER to cancel.");

    CTxDestination assetDest;
    CTxDestination changeDest;
    {
        LOCK(pwallet->cs_wallet);
        assetDest = NewMarketDest(pwallet, "Market cancel");
        changeDest = NewMarketDest(pwallet, "Market change");
        pwallet->UnlockCoin(pAsk->vin[0].prevout);
    }

    CMutableTransaction mtx;
    mtx.nVersion = CTransaction::CURRENT_VERSION;
    mtx.nLockTime = 0;
    mtx.vin.push_back(CTxIn(pAsk->vin[0].prevout, CScript(), CTxIn::SEQUENCE_FINAL));
    for (size_t i = 0; i < vPay.size(); ++i)
        mtx.vin.push_back(CTxIn(vPay[i], CScript(), CTxIn::SEQUENCE_FINAL));

    const CAmount nChange = nPayIn - MARKET_FEE;
    if (nChange > 0) {
        CTxOut changeOut(nChange, GetScriptForDestination(changeDest));
        if (!IsDust(changeOut, dustRelayFee))
            mtx.vout.push_back(changeOut);
    }

    CScript assetScript = GetScriptForDestination(assetDest);
    CAssetTransfer transfer(strAsset, nAmount);
    std::string transferErr;
    if (!transfer.IsValid(transferErr))
        throw JSONRPCError(RPC_WALLET_ERROR, transferErr);
    transfer.ConstructTransaction(assetScript);
    mtx.vout.push_back(CTxOut(0, assetScript));

    SignWalletTx(pwallet, mtx, SIGHASH_ALL);
    const uint256 txid = BroadcastRaw(mtx);
    xmarket::RemoveAsk(id);
    xmarket::PruneSpent();

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("txid", txid.GetHex());
    ret.pushKV("cancelled", id.GetHex());
    return ret;
}
#endif

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
    { "market",             "listmarket",             &listmarket,             {"asset"} },
#ifdef ENABLE_WALLET
    { "market",             "postask",                &postask,                {"asset", "price", "txid", "vout"} },
    { "market",             "takeask",                &takeask,                {"id"} },
    { "market",             "cancelask",              &cancelask,              {"id"} },
#endif
};

void RegisterMarketRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
