// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "lottery.h"
#include "pool.h"
#include "rpc/server.h"
#include "script/standard.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "xsession.h"

#include <univalue.h>

static CScript LocalPayoutScript()
{
    CScript script = lottery::GetRegistry().LocalScript();
    if (script.empty())
        script = lottery::LoadOrCreateLocalScript();
    return script;
}

static UniValue PublicAdvertObj(const xpool::Advert& a, bool includeId)
{
    UniValue o(UniValue::VOBJ);
    o.pushKV("name", a.name);
    if (includeId)
        o.pushKV("id", a.id);
    UniValue addrs(UniValue::VARR);
    for (size_t i = 0; i < a.members.size(); i++) {
        CTxDestination dest;
        if (ExtractDestination(a.members[i], dest))
            addrs.push_back(EncodeDestination(dest));
    }
    o.pushKV("addresses", addrs);
    o.pushKV("members", (int)a.members.size());
    const int64_t now = GetTime();
    o.pushKV("eligible_tickets", xpool::Get().EligibleTickets(a, lottery::GetRegistry().ActiveIds(now)));
    return o;
}

UniValue createpool(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error(
            "createpool \"name\" \"poolid\" \"password\"\n"
            "\nCreate a lottery pool from this wallet. GUI Home has the same buttons.\n"
            "Win chance is one ticket per X Verified eligible member who is running.\n"
            "Unverified members have zero tickets but still share a win evenly if they joined.\n"
            "Keep poolid + password private unless you want someone to join.\n"
            "Public display is the pool name and member payout addresses only.\n"
            "\nArguments:\n"
            "1. name      (string, required) public pool name\n"
            "2. poolid    (string, required) secret-ish id you can share with the password\n"
            "3. password  (string, required) join password; never gossiped\n"
        );

    std::string serr;
    if (!xsession::RequireSession(serr))
        throw JSONRPCError(RPC_INVALID_PARAMETER, serr);
    const CScript script = LocalPayoutScript();
    std::string err;
    if (!xpool::Get().Create(request.params[0].get_str(), request.params[1].get_str(),
                             request.params[2].get_str(), script, err))
        throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    xpool::Advert a;
    xpool::Get().GetById(request.params[1].get_str(), a);
    UniValue ret = PublicAdvertObj(a, true);
    ret.pushKV("created", UniValue(true));
    ret.pushKV("note", "Share pool id and password yourself. They are not shown on the public pool list.");
    return ret;
}

UniValue joinpool(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "joinpool \"poolid\" \"password\"\n"
            "\nJoin a pool with the id and password the creator shared. Adds this wallet's payout address.\n"
            "Unverified accounts can join and share wins, but they do not add lottery tickets.\n"
        );

    std::string serr;
    if (!xsession::RequireSession(serr))
        throw JSONRPCError(RPC_INVALID_PARAMETER, serr);
    const CScript script = LocalPayoutScript();
    std::string err;
    if (!xpool::Get().Join(request.params[0].get_str(), request.params[1].get_str(), script, err))
        throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    xpool::Advert a;
    xpool::Get().GetById(request.params[0].get_str(), a);
    UniValue ret = PublicAdvertObj(a, true);
    ret.pushKV("joined", UniValue(true));
    return ret;
}

UniValue leavepool(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "leavepool \"poolid\" ( \"password\" )\n"
            "\nLeave a pool. Password may be omitted if this node created or joined it.\n"
        );

    std::string serr;
    if (!xsession::RequireSession(serr))
        throw JSONRPCError(RPC_INVALID_PARAMETER, serr);
    const CScript script = LocalPayoutScript();
    const std::string pass = request.params.size() > 1 ? request.params[1].get_str() : "";
    std::string err;
    if (!xpool::Get().Leave(request.params[0].get_str(), pass, script, err))
        throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    UniValue ret(UniValue::VOBJ);
    ret.pushKV("left", UniValue(true));
    return ret;
}

UniValue listpools(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "listpools\n"
            "\nPublic pool view: name and member payout addresses only. No pool id, no password.\n"
        );

    UniValue arr(UniValue::VARR);
    std::vector<xpool::Advert> all = xpool::Get().AllAdverts();
    for (size_t i = 0; i < all.size(); i++)
        arr.push_back(PublicAdvertObj(all[i], false));
    return arr;
}

UniValue getmypool(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "getmypool\n"
            "\nThis wallet's pool, if any. Includes pool id so you can share it with the password.\n"
            "Password is never returned.\n"
        );

    const std::string id = xpool::Get().LocalPoolId();
    if (id.empty()) {
        UniValue o(UniValue::VOBJ);
        o.pushKV("in_pool", UniValue(false));
        return o;
    }
    xpool::Advert a;
    xpool::Get().GetById(id, a);
    UniValue o = PublicAdvertObj(a, xpool::Get().HasSecret(id));
    o.pushKV("in_pool", UniValue(true));
    o.pushKV("has_password", UniValue(xpool::Get().HasSecret(id)));
    return o;
}

static const CRPCCommand commands[] =
{
    { "lottery", "createpool", &createpool, {"name", "poolid", "password"} },
    { "lottery", "joinpool",   &joinpool,   {"poolid", "password"} },
    { "lottery", "leavepool",  &leavepool,  {"poolid", "password"} },
    { "lottery", "listpools",  &listpools,  {} },
    { "lottery", "getmypool",  &getmypool,  {} },
};

void RegisterPoolRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
