// Copyright (c) 2026 The X Coin developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assets/xaccount.h"
#include "base58.h"
#include "chain.h"
#include "chainparams.h"
#include "lottery.h"
#include "rpc/server.h"
#include "script/standard.h"
#include "util.h"
#include "utilstrencodings.h"
#include "validation.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <univalue.h>

UniValue getlotteryinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "getlotteryinfo\n"
            "\nReturn the current lottery draw for the next block.\n"
            "X Coin produces blocks by a minute lottery among verified-X active nodes, not PoW.\n"
            "A node with no linked X account, or whose account is not on the verified allowlist,\n"
            "is not lottery-eligible (it may still sync and relay).\n"
            "\nResult:\n"
            "{\n"
            "  \"height\": n,                 (numeric) next block height\n"
            "  \"slot\": n,                   (numeric) lottery minute/slot\n"
            "  \"slot_seconds\": 60,          (numeric) seconds per slot\n"
            "  \"winner_count\": n,           (numeric) winners this slot (grows on halvings)\n"
            "  \"halving_interval\": n,       (numeric) heights per subsidy/winner step\n"
            "  \"active_nodes\": n,           (numeric) verified-X nodes with a fresh heartbeat\n"
            "  \"local_id\": \"hex\",           (string) Hash160 of this node's payout script\n"
            "  \"local_xaccount\": \"handle\",  (string) linked X handle (empty if none)\n"
            "  \"local_xuserid\": n,          (numeric) optional numeric X user id\n"
            "  \"local_eligible\": true|false,(boolean) linked and on the verified allowlist\n"
            "  \"local_is_winner\": true|false,\n"
            "  \"verified_accounts\": n,      (numeric) size of the operator allowlist\n"
            "  \"seed\": \"hex\",               (string) deterministic seed\n"
            "  \"winners\": [\"hex\", ...],     (array) selected node ids\n"
            "  \"rewards\": [n, ...],         (array) xferon amounts per winner (subsidy only)\n"
            "  \"producer_running\": true|false\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getlotteryinfo", "")
            + HelpExampleRpc("getlotteryinfo", "")
        );

    LOCK(cs_main);
    const CChainParams& params = GetParams();
    const Consensus::Params& consensus = params.GetConsensus();
    CBlockIndex* tip = chainActive.Tip();
    const int nextHeight = tip ? tip->nHeight + 1 : 1;
    const uint256 prev = tip ? tip->GetBlockHash() : params.GenesisBlock().GetHash();
    const int64_t now = GetTime();
    lottery::GetRegistry().HeartbeatLocal(now);
    CAmount subsidy = GetBlockSubsidy(nextHeight, consensus);
    lottery::Draw draw = lottery::ComputeDraw(nextHeight, prev,
                                              params.GenesisBlock().nTime,
                                              consensus.nSubsidyHalvingInterval,
                                              subsidy, now);
    const lottery::XAccount localX = lottery::GetRegistry().LocalXAccount();

    UniValue winners(UniValue::VARR);
    for (const auto& id : draw.winners)
        winners.push_back(id.GetHex());
    UniValue rewards(UniValue::VARR);
    for (const auto& amt : draw.rewards)
        rewards.push_back(amt);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("height", nextHeight));
    ret.push_back(Pair("slot", draw.slot));
    ret.push_back(Pair("slot_seconds", (int64_t)lottery::SLOT_SECONDS));
    ret.push_back(Pair("winner_count", draw.winnerCount));
    ret.push_back(Pair("halving_interval", consensus.nSubsidyHalvingInterval));
    ret.push_back(Pair("active_nodes", (int)draw.active.size()));
    ret.push_back(Pair("local_id", lottery::GetRegistry().LocalId().GetHex()));
    ret.push_back(Pair("local_xaccount", localX.handle));
    ret.push_back(Pair("local_xuserid", (uint64_t)localX.userId));
    ret.push_back(Pair("local_eligible", lottery::GetRegistry().LocalEligible()));
    ret.push_back(Pair("local_is_winner", lottery::IsWinner(lottery::GetRegistry().LocalId(), draw.winners)));
    ret.push_back(Pair("verified_accounts", (int)lottery::GetAllowlist().Size()));
    ret.push_back(Pair("seed", draw.seed.GetHex()));
    ret.push_back(Pair("winners", winners));
    ret.push_back(Pair("rewards", rewards));
    ret.push_back(Pair("producer_running", lottery::IsProducerRunning()));
    ret.push_back(Pair("currency", std::string("XFER")));
    ret.push_back(Pair("subunit", std::string("xferon")));
    return ret;
}

UniValue getactivenodes(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "getactivenodes\n"
            "\nList lottery-eligible active nodes (local registry, filled by heartbeats and P2P xhb gossip).\n"
            "A node is active if it heartbeated a verified X identity within the last 180 seconds.\n"
            "Unlinked or unverified heartbeats are ignored. id is Hash160(payout script).\n"
            "\nResult:\n"
            "[\n"
            "  {\"id\":\"hex\", \"script\":\"hex\", \"lastseen\": n, \"local\": bool,\n"
            "   \"xaccount\":\"handle\", \"xuserid\": n}\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getactivenodes", "")
            + HelpExampleRpc("getactivenodes", "")
        );

    const int64_t now = GetTime();
    lottery::GetRegistry().HeartbeatLocal(now);
    UniValue ret(UniValue::VARR);
    for (const auto& n : lottery::GetRegistry().ActiveNodes(now)) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("id", n.id.GetHex()));
        obj.push_back(Pair("script", HexStr(n.script.begin(), n.script.end())));
        obj.push_back(Pair("lastseen", n.lastSeen));
        obj.push_back(Pair("local", n.id == lottery::GetRegistry().LocalId()));
        obj.push_back(Pair("xaccount", n.x.handle));
        obj.push_back(Pair("xuserid", (uint64_t)n.x.userId));
        ret.push_back(obj);
    }
    return ret;
}

static CScript ParsePayoutArg(const std::string& s)
{
    if (IsHex(s) && s.size() >= 4 && (s.size() % 2) == 0 && s.size() != 40) {
        std::vector<unsigned char> raw = ParseHex(s);
        if (raw.empty() || raw.size() > lottery::MAX_HEARTBEAT_SCRIPT)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "script hex empty or too large");
        return CScript(raw.begin(), raw.end());
    }
    CTxDestination dest = DecodeDestination(s);
    if (!IsValidDestination(dest))
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "pass a payout address or script hex (not a bare 40-hex id)");
    return GetScriptForDestination(dest);
}

UniValue registeractivenode(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error(
            "registeractivenode ( payout xaccount xuserid )\n"
            "\nRecord a heartbeat for an active node. Requires a verified X identity.\n"
            "With no arguments, heartbeats this node using -xaccount / -xuserid.\n"
            "A payout without xaccount uses the local linked handle (one X account = one node).\n"
            "Pass a different allowlisted xaccount to register another verified identity.\n"
            "Heartbeats also gossip to peers as `xhb` (timestamp + script + X identity).\n"
            "Unlinked or unverified registrations are rejected.\n"
            "\nArguments:\n"
            "1. payout    (string, optional) X Coin address or script hex. Default: local payout script.\n"
            "2. xaccount  (string, optional) X handle. Default: this node's -xaccount.\n"
            "3. xuserid   (numeric, optional) numeric X user id. Default: -xuserid or 0.\n"
            "\nResult:\n"
            "{ \"id\": \"hex\", \"script\": \"hex\", \"lastseen\": n, \"xaccount\": \"handle\",\n"
            "  \"xuserid\": n, \"active_nodes\": n }\n"
            "\nExamples:\n"
            + HelpExampleCli("registeractivenode", "")
            + HelpExampleCli("registeractivenode", "\"yLocalAddress\" \"alice\"")
            + HelpExampleRpc("registeractivenode", "")
        );

    CScript script = lottery::GetRegistry().LocalScript();
    lottery::XAccount x = lottery::GetRegistry().LocalXAccount();
    if (!request.params[0].isNull())
        script = ParsePayoutArg(request.params[0].get_str());
    if (script.empty())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "no payout script available");

    if (!request.params[1].isNull()) {
        std::string err;
        if (!lottery::NormalizeXHandle(request.params[1].get_str(), x.handle, err))
            throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    }
    if (request.params.size() > 2 && !request.params[2].isNull()) {
        if (!request.params[2].isNum() && !request.params[2].isStr())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "xuserid must be a number");
        const int64_t uid = request.params[2].isNum() ? request.params[2].get_int64()
                                                      : atoi64(request.params[2].get_str());
        if (uid < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "xuserid must be >= 0");
        x.userId = (uint64_t)uid;
    }

    if (x.handle.empty())
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "no X account linked; set -xaccount=handle in xcoin.conf or pass xaccount");
    if (!lottery::GetAllowlist().Contains(x.handle, x.userId))
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "X account is not on the verified allowlist (addxverified / -xverified / allowlist file)");

    const int64_t now = GetTime();
    if (!lottery::GetRegistry().Heartbeat(script, now, x.handle, x.userId))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "heartbeat rejected (unlinked or unverified X account)");
    const uint160 id = lottery::IdFromScript(script);
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("id", id.GetHex()));
    ret.push_back(Pair("script", HexStr(script.begin(), script.end())));
    ret.push_back(Pair("lastseen", now));
    ret.push_back(Pair("xaccount", x.handle));
    ret.push_back(Pair("xuserid", (uint64_t)x.userId));
    ret.push_back(Pair("active_nodes", (int)lottery::GetRegistry().Count(now)));
#ifdef ENABLE_WALLET
    {
        std::string dest;
        CTxDestination d;
        if (ExtractDestination(script, d))
            dest = EncodeDestination(d);
        std::string aerr, aname, atxid;
        if (AssignLinkedUserMainAsset(x.handle, dest, aerr, &aname, &atxid)) {
            ret.push_back(Pair("main_asset", aname));
            if (!atxid.empty())
                ret.push_back(Pair("main_asset_txid", atxid));
        }
    }
#endif
    return ret;
}

UniValue addxverified(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "addxverified \"handle\" ( userid )\n"
            "\nAdd an X handle to the operator-shared verified allowlist.\n"
            "Honest operators must only list X-verified (blue-check / X Premium verified) accounts.\n"
            "All honest nodes must share the same list so they agree on the active set.\n"
            "The list is written to verified-x-accounts.txt in the data directory.\n"
            "\nArguments:\n"
            "1. handle  (string, required) X handle (with or without @)\n"
            "2. userid  (numeric, optional) numeric X user id\n"
            "\nResult:\n"
            "{ \"handle\": \"alice\", \"userid\": n, \"verified_accounts\": n }\n"
            "\nExamples:\n"
            + HelpExampleCli("addxverified", "alice")
            + HelpExampleRpc("addxverified", "\"alice\"")
        );

    std::string handle;
    std::string err;
    if (!lottery::NormalizeXHandle(request.params[0].get_str(), handle, err))
        throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    uint64_t uid = 0;
    if (request.params.size() > 1 && !request.params[1].isNull()) {
        const int64_t v = request.params[1].isNum() ? request.params[1].get_int64()
                                                    : atoi64(request.params[1].get_str());
        if (v < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "userid must be >= 0");
        uid = (uint64_t)v;
    }
    if (!lottery::GetAllowlist().Add(handle, uid, err))
        throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("handle", handle));
    ret.push_back(Pair("userid", (uint64_t)uid));
    ret.push_back(Pair("verified_accounts", (int)lottery::GetAllowlist().Size()));
    return ret;
}

UniValue removexverified(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "removexverified \"handle\"\n"
            "\nRemove an X handle from the verified allowlist.\n"
            "\nArguments:\n"
            "1. handle  (string, required) X handle\n"
            "\nExamples:\n"
            + HelpExampleCli("removexverified", "alice")
            + HelpExampleRpc("removexverified", "\"alice\"")
        );

    std::string err;
    if (!lottery::GetAllowlist().Remove(request.params[0].get_str(), err))
        throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("removed", request.params[0].get_str()));
    ret.push_back(Pair("verified_accounts", (int)lottery::GetAllowlist().Size()));
    return ret;
}

UniValue listxverified(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "listxverified\n"
            "\nList the operator-shared verified X allowlist.\n"
            "Only these handles may enter the lottery active set.\n"
            "\nResult:\n"
            "[ {\"handle\":\"alice\", \"userid\": n}, ... ]\n"
            "\nExamples:\n"
            + HelpExampleCli("listxverified", "")
            + HelpExampleRpc("listxverified", "")
        );

    UniValue ret(UniValue::VARR);
    for (const auto& a : lottery::GetAllowlist().List()) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("handle", a.handle));
        obj.push_back(Pair("userid", (uint64_t)a.userId));
        ret.push_back(obj);
    }
    return ret;
}

UniValue loadxverified(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "loadxverified ( path )\n"
            "\nReload / merge verified X accounts from a text file (no X API keys).\n"
            "Each line is `handle` or `handle userid`. Lines starting with # are comments.\n"
            "Default path is verified-x-accounts.txt in the data directory.\n"
            "Operators can refresh a published list with curl, then call this RPC:\n"
            "  curl -fsSL https://example.com/verified-x-accounts.txt -o ~/.xcoin/verified-x-accounts.txt\n"
            "  xcoin-cli loadxverified\n"
            "\nArguments:\n"
            "1. path  (string, optional) file to merge. Default: datadir allowlist file.\n"
            "\nExamples:\n"
            + HelpExampleCli("loadxverified", "")
            + HelpExampleCli("loadxverified", "/shared/verified-x-accounts.txt")
        );

    std::string path = lottery::GetAllowlist().PersistPath();
    if (!request.params[0].isNull())
        path = request.params[0].get_str();
    if (path.empty())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "no allowlist path configured");
    std::string err;
    if (!lottery::GetAllowlist().LoadFile(path, err))
        throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    if (!lottery::GetAllowlist().Persist(err))
        throw JSONRPCError(RPC_MISC_ERROR, err);
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("path", path));
    ret.push_back(Pair("verified_accounts", (int)lottery::GetAllowlist().Size()));
    return ret;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
    { "lottery",            "getlotteryinfo",         &getlotteryinfo,         {} },
    { "lottery",            "getactivenodes",         &getactivenodes,         {} },
    { "lottery",            "registeractivenode",     &registeractivenode,     {"payout", "xaccount", "xuserid"} },
    { "lottery",            "addxverified",           &addxverified,           {"handle", "userid"} },
    { "lottery",            "removexverified",        &removexverified,        {"handle"} },
    { "lottery",            "listxverified",          &listxverified,          {} },
    { "lottery",            "loadxverified",          &loadxverified,          {"path"} },
};

void RegisterLotteryRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
