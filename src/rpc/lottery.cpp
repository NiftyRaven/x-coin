// Copyright (c) 2026 The X Coin developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
            "X Coin produces blocks by a minute lottery among active nodes, not PoW.\n"
            "\nResult:\n"
            "{\n"
            "  \"height\": n,                 (numeric) next block height\n"
            "  \"slot\": n,                   (numeric) lottery minute/slot\n"
            "  \"slot_seconds\": 60,          (numeric) seconds per slot\n"
            "  \"winner_count\": n,           (numeric) winners this slot (grows on halvings)\n"
            "  \"halving_interval\": n,       (numeric) heights per subsidy/winner step\n"
            "  \"active_nodes\": n,           (numeric) nodes with a fresh heartbeat\n"
            "  \"local_id\": \"hex\",           (string) Hash160 of this node's payout script\n"
            "  \"local_is_winner\": true|false,\n"
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
    ret.push_back(Pair("local_is_winner", lottery::IsWinner(lottery::GetRegistry().LocalId(), draw.winners)));
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
            "\nList active lottery nodes (local registry, filled by heartbeats and P2P xhb gossip).\n"
            "A node is active if it has heartbeated within the last 180 seconds.\n"
            "id is Hash160(payout script).\n"
            "\nResult:\n"
            "[\n"
            "  {\"id\":\"hex\", \"script\":\"hex\", \"lastseen\": n, \"local\": bool}\n"
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
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "registeractivenode ( payout )\n"
            "\nRecord a heartbeat for an active node. With no argument, heartbeats this node.\n"
            "Heartbeats also gossip to peers as `xhb` (timestamp + payout script).\n"
            "\nArguments:\n"
            "1. payout  (string, optional) X Coin address or script hex. Default: local payout script.\n"
            "\nResult:\n"
            "{ \"id\": \"hex\", \"script\": \"hex\", \"lastseen\": n, \"active_nodes\": n }\n"
            "\nExamples:\n"
            + HelpExampleCli("registeractivenode", "")
            + HelpExampleRpc("registeractivenode", "")
        );

    CScript script = lottery::GetRegistry().LocalScript();
    if (!request.params[0].isNull())
        script = ParsePayoutArg(request.params[0].get_str());
    if (script.empty())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "no payout script available");

    const int64_t now = GetTime();
    lottery::GetRegistry().Heartbeat(script, now);
    const uint160 id = lottery::IdFromScript(script);
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("id", id.GetHex()));
    ret.push_back(Pair("script", HexStr(script.begin(), script.end())));
    ret.push_back(Pair("lastseen", now));
    ret.push_back(Pair("active_nodes", (int)lottery::GetRegistry().Count(now)));
    return ret;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
    { "lottery",            "getlotteryinfo",         &getlotteryinfo,         {} },
    { "lottery",            "getactivenodes",         &getactivenodes,         {} },
    { "lottery",            "registeractivenode",     &registeractivenode,     {"payout"} },
};

void RegisterLotteryRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
