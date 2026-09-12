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
#include "xrelease.h"
#include "xsession.h"

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
            "X Coin produces blocks by a minute lottery among X Verified active nodes, not PoW.\n"
            "X Verified is X's blue check / X Premium (and business / government org checks)\n"
            "from GET /2/users/me — https://help.x.com/en/managing-your-account/about-x-bluecheck\n"
            "A node with no Sign in with X session, or whose session is not X Verified,\n"
            "is not lottery-eligible (zero chance). It may still sync, relay, send, and receive.\n"
            "An X Verified session plus a running wallet cannot be excluded from the draw.\n"
            "The operator invite list is optional pins / invites, not a lottery gate.\n"
            "\nResult:\n"
            "{\n"
            "  \"height\": n,                 (numeric) next block height\n"
            "  \"slot\": n,                   (numeric) lottery minute/slot\n"
            "  \"slot_seconds\": 60,          (numeric) seconds per slot\n"
            "  \"winner_count\": n,           (numeric) winners this slot (grows on halvings)\n"
            "  \"halving_interval\": n,       (numeric) heights per subsidy/winner step\n"
            "  \"active_nodes\": n,           (numeric) X Verified nodes with a fresh heartbeat\n"
            "  \"local_id\": \"hex\",           (string) Hash160 of this node's payout script\n"
            "  \"local_xaccount\": \"handle\",  (string) linked X handle (empty if none)\n"
            "  \"local_xuserid\": n,          (numeric) optional numeric X user id\n"
            "  \"local_x_verified\": true|false,(boolean) session users/me.verified (blue check)\n"
            "  \"local_eligible\": true|false,(boolean) X Verified (blue check) + running wallet\n"
            "  \"attestation_required\": true|false, (boolean) main/test: XHB1 needs seed XVA1 stamps\n"
            "  \"local_attested\": true|false, (boolean) seed has stamped this payout as a live blue check\n"
            "  \"x_lookup\": true|false,       (boolean) this node calls X to check actual blue checks\n"
            "  \"local_is_winner\": true|false,\n"
            "  \"allowlist_accounts\": n,     (numeric) operator invite list size (not X Verified)\n"
            "  \"verified_accounts\": n,      (numeric) deprecated alias of allowlist_accounts\n"
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
    ret.push_back(Pair("next_draw_height", nextHeight));
    ret.push_back(Pair("slot", draw.slot));
    ret.push_back(Pair("slot_seconds", (int64_t)lottery::SLOT_SECONDS));
    ret.push_back(Pair("winner_count", draw.winnerCount));
    ret.push_back(Pair("halving_interval", consensus.nSubsidyHalvingInterval));
    ret.push_back(Pair("active_nodes", (int)draw.active.size()));
    ret.push_back(Pair("local_id", lottery::GetRegistry().LocalId().GetHex()));
    ret.push_back(Pair("local_xaccount", localX.handle));
    ret.push_back(Pair("local_xuserid", (uint64_t)localX.userId));
    ret.push_back(Pair("local_eligible", lottery::GetRegistry().LocalEligible()));
    ret.push_back(Pair("local_x_verified", xsession::SessionIsXVerified()));
    ret.push_back(Pair("local_is_winner", lottery::IsWinner(lottery::GetRegistry().LocalId(), draw.winners)));
    ret.push_back(Pair("allowlist_accounts", (int)lottery::GetAllowlist().Size()));
    ret.push_back(Pair("verified_accounts", (int)lottery::GetAllowlist().Size()));
    ret.push_back(Pair("seed", draw.seed.GetHex()));
    ret.push_back(Pair("winners", winners));
    ret.push_back(Pair("rewards", rewards));
    ret.push_back(Pair("producer_running", lottery::IsProducerRunning()));
    ret.push_back(Pair("attestation_required", lottery::RequireXAttestation()));
    ret.push_back(Pair("local_attested", lottery::HasAttestation(lottery::GetRegistry().LocalId())));
    ret.push_back(Pair("x_lookup", lottery::IsXLookupNode()));
    ret.push_back(Pair("baked_seed", lottery::IsBakedSeed()));
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
            "A node is active if it heartbeated an X Verified identity within the last 180 seconds.\n"
            "Unlinked or unverified (no X blue check) local heartbeats are ignored. id is Hash160(payout script).\n"
            "\nResult:\n"
            "[\n"
            "  {\"id\":\"hex\", \"script\":\"hex\", \"lastseen\": n, \"local\": bool,\n"
            "   \"xaccount\":\"handle\"}\n"
            "Numeric X user ids are not gossiped and are not listed here.\n"
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
        obj.push_back(Pair("attested", lottery::HasAttestation(n.id)));
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
            "\nRecord a heartbeat for an active node. Requires a Sign in with X session\n"
            "that is X Verified (blue check from GET /2/users/me). Unverified sessions\n"
            "have zero chance. The operator invite list cannot exclude a verified wallet.\n"
            "The handle and user id must match the signed-in session. Typed foreign\n"
            "handles are rejected. Gossip heartbeats still need a payout-key signature;\n"
            "this RPC is the local, session-trusted path.\n"
            "With no arguments, heartbeats this node's signed-in identity.\n"
            "\nArguments:\n"
            "1. payout    (string, optional) X Coin address or script hex. Default: local payout script.\n"
            "2. xaccount  (string, optional) X handle. Default: signed-in username.\n"
            "3. xuserid   (numeric, optional) numeric X user id. Must match the session.\n"
            "\nResult:\n"
            "{ \"id\": \"hex\", \"script\": \"hex\", \"lastseen\": n, \"xaccount\": \"handle\",\n"
            "  \"xuserid\": n, \"active_nodes\": n }\n"
            "\nExamples:\n"
            + HelpExampleCli("registeractivenode", "")
            + HelpExampleCli("registeractivenode", "\"yLocalAddress\" \"alice\"")
            + HelpExampleRpc("registeractivenode", "")
        );

    std::string serr;
    if (!xsession::RequireSession(serr))
        throw JSONRPCError(RPC_INVALID_PARAMETER, serr);
    if (!xsession::RequireXVerified(serr))
        throw JSONRPCError(RPC_INVALID_PARAMETER, serr);

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
    if (x.handle.empty())
        x.handle = xsession::SignedInHandle();
    if (!xsession::RequireHandle(x.handle, serr))
        throw JSONRPCError(RPC_INVALID_PARAMETER, serr);

    const std::string sid = xsession::SignedInUserId();
    const uint64_t sessionUid = sid.empty() ? 0 : (uint64_t)atoi64(sid);
    if (request.params.size() > 2 && !request.params[2].isNull()) {
        if (!request.params[2].isNum() && !request.params[2].isStr())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "xuserid must be a number");
        const int64_t uid = request.params[2].isNum() ? request.params[2].get_int64()
                                                      : atoi64(request.params[2].get_str());
        if (uid < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "xuserid must be >= 0");
        if (sessionUid != 0 && (uint64_t)uid != sessionUid)
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               "xuserid must match the signed-in X user id");
        x.userId = (uint64_t)uid;
    }
    if (x.userId == 0)
        x.userId = sessionUid;

    const int64_t now = GetTime();
    if (!lottery::GetRegistry().Heartbeat(script, now, x.handle, x.userId, true))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "heartbeat rejected (payout pin or live-handle binding)");
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

UniValue getxsession(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "getxsession\n"
            "\nReturn the local Sign in with X session on this node only.\n"
            "This RPC is localhost-authenticated. Do not publish the result.\n"
            "It does not return X login tokens (tokens are never saved).\n"
            "signed_in / linked are the same: this datadir holds a valid HMAC proof\n"
            "binding send/receive/own to that X account.\n"
            "verified / verified_type are what GET /2/users/me reported.\n"
            "x_verified is true for X's blue / business / government checks.\n"
            "This is not a replacement for the 12-word BIP39 seed.\n"
        );
    UniValue ret(UniValue::VOBJ);
    xsession::Session s;
    std::string err;
    const bool ok = xsession::LoadSession(s, err);
    ret.push_back(Pair("signed_in", ok));
    ret.push_back(Pair("linked", ok));
    ret.push_back(Pair("session_file", xsession::SessionPath()));
    ret.push_back(Pair("secret_file", xsession::SecretPath()));
    if (ok) {
        ret.push_back(Pair("username", s.username));
        ret.push_back(Pair("id", s.userId));
        ret.push_back(Pair("user_id", s.userId));
        ret.push_back(Pair("expires", s.expiresAt));
        ret.push_back(Pair("verified", s.verified));
        ret.push_back(Pair("verified_type", s.verifiedType));
        ret.push_back(Pair("x_verified", s.IsXVerified()));
    } else {
        ret.push_back(Pair("error", err));
    }
    return ret;
}

UniValue getreleasenotes(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "getreleasenotes ( json )\n"
            "\nReturn What's new for this wallet, and optionally parse a\n"
            "GitHub Releases JSON feed (no network). The GUI fetches the feed\n"
            "and shows notes in Help → What's new / the Home banner.\n"
            "\nArguments:\n"
            "1. json    (string, optional) GitHub releases array or {releases:[...]}\n"
        );
    UniValue ret(UniValue::VOBJ);
    const xrelease::Release bundled = xrelease::BundledRelease();
    ret.push_back(Pair("version", xrelease::RunningVersionString()));
    ret.push_back(Pair("tag", bundled.tag));
    ret.push_back(Pair("client_version", bundled.version));
    ret.push_back(Pair("name", bundled.name));
    ret.push_back(Pair("notes", bundled.body));
    ret.push_back(Pair("url", bundled.htmlUrl));
    ret.push_back(Pair("feed_url", xrelease::FeedUrl()));
    ret.push_back(Pair("check_enabled", xrelease::CheckEnabled()));
    if (request.params.size() == 1) {
        std::vector<xrelease::Release> all;
        std::string err;
        const std::string json = request.params[0].isStr()
            ? request.params[0].get_str()
            : request.params[0].write();
        if (!xrelease::ParseReleaseFeed(json, all, err))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, err);
        UniValue arr(UniValue::VARR);
        for (const xrelease::Release& r : all)
            arr.push_back(xrelease::ToUniValue(r));
        ret.push_back(Pair("releases", arr));
        xrelease::Release newer;
        if (xrelease::LatestNewer(all, newer))
            ret.push_back(Pair("newer", xrelease::ToUniValue(newer)));
        else
            ret.push_back(Pair("newer", NullUniValue));
    }
    return ret;
}

UniValue mockxsignin(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 1)
        throw std::runtime_error(
            "mockxsignin \"payload\"\n"
            "\nREGTEST ONLY. Inject a mock GET /2/users/me payload and write a session proof.\n"
            "payload is JSON {\"data\":{\"id\":\"…\",\"username\":\"…\",\"verified\":true,\"verified_type\":\"blue\"}}\n"
            "or handle[:userid][:verified|:unverified|:blue|:business|:government].\n"
            "Examples: NFTRVN:verified   ghost:unverified   alice:99:verified\n"
            "Compact NFTRVN with no flag defaults to verified=true (regtest).\n"
        );
    if (!xsession::IsRegtest())
        throw JSONRPCError(RPC_MISC_ERROR, "mockxsignin is only available on -regtest");
    std::string err;
    UniValue parsed;
    if (!xsession::ApplyUsersMePayload(request.params[0].get_str(), err, &parsed))
        throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("ok", true));
    ret.push_back(Pair("username", xsession::SignedInHandle()));
    ret.push_back(Pair("id", xsession::SignedInUserId()));
    ret.push_back(Pair("verified", xsession::SignedInVerified()));
    ret.push_back(Pair("verified_type", xsession::SignedInVerifiedType()));
    ret.push_back(Pair("x_verified", xsession::SessionIsXVerified()));
    if (parsed.isObject())
        ret.push_back(Pair("users_me", parsed));
    return ret;
}

UniValue addxverified(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "addxverified \"handle\" ( userid )\n"
            "\nAdd an X handle to the operator invite list (private-mesh ACL).\n"
            "This is NOT X Verified. X Verified is X's blue check from GET /2/users/me\n"
            "(https://help.x.com/en/managing-your-account/about-x-bluecheck).\n"
            "This list cannot exclude an X Verified wallet from the lottery.\n"
            "Optional payout pins still bind a handle to one script.\n"
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
            "\nRemove an X handle from the operator invite list (not X Verified).\n"
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
            "\nList the operator invite list (private-mesh ACL). This is not X Verified.\n"
            "Not a lottery gate: X Verified + a running wallet is enough to enter.\n"
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
            "\nReload / merge the operator invite list from a text file (no X API keys).\n"
            "This file is not X Verified — it is a private-mesh ACL / invite list.\n"
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
    { "lottery",            "getxsession",            &getxsession,            {} },
    { "lottery",            "getreleasenotes",        &getreleasenotes,        {"json"} },
    { "lottery",            "mockxsignin",            &mockxsignin,            {"payload"} },
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
