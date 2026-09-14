// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hostshare.h"

#include "assets/assetdb.h"
#include "assets/xaccount.h"
#include "base58.h"
#include "fs.h"
#include "lottery.h"
#include "script/standard.h"
#include "sync.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validation.h"
#include "xsession.h"

#include <univalue.h>

#include <algorithm>
#include <cstdio>
#include <set>

#ifdef ENABLE_WALLET
#include "consensus/validation.h"
#include "net.h"
#include "policy/policy.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"
#endif

namespace hostshare {

static CCriticalSection cs_hostshare;
static bool g_enabled = false;
static int g_percent = DEFAULT_PERCENT;
static std::vector<std::string> g_guests;
static std::set<std::string> g_shared;

static fs::path StatePath()
{
    return GetDataDir() / "host-guests.json";
}

static fs::path ReindexMarkerPath()
{
    return GetDataDir() / "request-reindex";
}

static bool PersistLocked()
{
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("enabled", g_enabled));
    obj.push_back(Pair("guest_percent", g_percent));
    UniValue guests(UniValue::VARR);
    for (const auto& h : g_guests)
        guests.push_back(h);
    obj.push_back(Pair("guests", guests));
    UniValue shared(UniValue::VARR);
    size_t n = 0;
    for (const auto& txid : g_shared) {
        if (n++ >= 256)
            break;
        shared.push_back(txid);
    }
    obj.push_back(Pair("shared_coinbases", shared));

    FILE* f = fsbridge::fopen(StatePath(), "wb");
    if (!f)
        return false;
    const std::string json = obj.write(2) + "\n";
    const size_t wrote = fwrite(json.data(), 1, json.size(), f);
    fclose(f);
    return wrote == json.size();
}

static void LoadLocked()
{
    g_enabled = false;
    g_percent = DEFAULT_PERCENT;
    g_guests.clear();
    g_shared.clear();
    FILE* f = fsbridge::fopen(StatePath(), "rb");
    if (!f)
        return;
    std::string raw;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        raw.append(buf, n);
    fclose(f);
    UniValue obj;
    if (!obj.read(raw) || !obj.isObject())
        return;
    if (obj.exists("enabled"))
        g_enabled = obj["enabled"].isTrue();
    if (obj.exists("guest_percent") && obj["guest_percent"].isNum()) {
        const int p = obj["guest_percent"].get_int();
        if (p >= MIN_PERCENT && p <= MAX_PERCENT)
            g_percent = p;
    }
    if (obj.exists("guests") && obj["guests"].isArray()) {
        const UniValue& arr = obj["guests"];
        for (size_t i = 0; i < arr.size() && g_guests.size() < MAX_GUESTS; i++) {
            std::string norm;
            std::string err;
            if (lottery::NormalizeXHandle(arr[i].getValStr(), norm, err)) {
                if (std::find(g_guests.begin(), g_guests.end(), norm) == g_guests.end())
                    g_guests.push_back(norm);
            }
        }
    }
    if (obj.exists("shared_coinbases") && obj["shared_coinbases"].isArray()) {
        const UniValue& arr = obj["shared_coinbases"];
        for (size_t i = 0; i < arr.size(); i++)
            g_shared.insert(arr[i].getValStr());
    }
}

void Load()
{
    LOCK(cs_hostshare);
    LoadLocked();
}

void ResetForTest()
{
    LOCK(cs_hostshare);
    g_enabled = false;
    g_percent = DEFAULT_PERCENT;
    g_guests.clear();
    g_shared.clear();
}

bool Enabled()
{
    LOCK(cs_hostshare);
    return g_enabled;
}

int Percent()
{
    LOCK(cs_hostshare);
    return g_percent;
}

bool AssetIndexReady()
{
    return fAssetIndex && passetsdb != nullptr;
}

bool SetEnabled(bool on, std::string& err)
{
    err.clear();
    if (on && !xsession::SessionIsXVerified()) {
        err = "Share lottery wins requires an X Verified session";
        return false;
    }
    LOCK(cs_hostshare);
    g_enabled = on;
    PersistLocked();
    return true;
}

bool SetPercent(int percent, std::string& err)
{
    err.clear();
    if (percent < MIN_PERCENT || percent > MAX_PERCENT) {
        err = strprintf("guest percent must be %d–%d", MIN_PERCENT, MAX_PERCENT);
        return false;
    }
    LOCK(cs_hostshare);
    g_percent = percent;
    PersistLocked();
    return true;
}

static bool AlreadyInvitedLocked(const std::string& handle)
{
    return std::find(g_guests.begin(), g_guests.end(), handle) != g_guests.end();
}

bool InviteGuest(const std::string& handleIn, std::string& err)
{
    err.clear();
    if (!xsession::SessionIsXVerified()) {
        err = "Only an X Verified host can invite guests";
        return false;
    }
    std::string handle;
    if (!lottery::NormalizeXHandle(handleIn, handle, err))
        return false;
    const lottery::XAccount local = lottery::GetRegistry().LocalXAccount();
    if (!local.handle.empty() && local.handle == handle) {
        err = "Cannot invite your own handle";
        return false;
    }
    std::string signedIn = xsession::SignedInHandle();
    std::string signedNorm;
    std::string nerr;
    if (!signedIn.empty() && lottery::NormalizeXHandle(signedIn, signedNorm, nerr) && signedNorm == handle) {
        err = "Cannot invite your own handle";
        return false;
    }
    std::string asset;
    if (!CheckIfXAccountAssigned(handle, &asset)) {
        err = strprintf("@%s has not claimed a root asset. They must Sign in with X and Claim my root asset first.", handle);
        return false;
    }
    LOCK(cs_hostshare);
    if (AlreadyInvitedLocked(handle)) {
        err = strprintf("@%s is already a guest", handle);
        return false;
    }
    if (g_guests.size() >= MAX_GUESTS) {
        err = strprintf("Guest list is full (%u)", (unsigned)MAX_GUESTS);
        return false;
    }
    g_guests.push_back(handle);
    PersistLocked();
    return true;
}

bool RemoveGuest(const std::string& handleIn, std::string& err)
{
    err.clear();
    std::string handle;
    if (!lottery::NormalizeXHandle(handleIn, handle, err))
        return false;
    LOCK(cs_hostshare);
    auto it = std::find(g_guests.begin(), g_guests.end(), handle);
    if (it == g_guests.end()) {
        err = strprintf("@%s is not a guest", handle);
        return false;
    }
    g_guests.erase(it);
    PersistLocked();
    return true;
}

CAmount GuestPot(CAmount nValue, int percent)
{
    if (nValue <= 0 || percent < MIN_PERCENT)
        return 0;
    if (percent > MAX_PERCENT)
        percent = MAX_PERCENT;
    return (nValue / 100) * percent + ((nValue % 100) * percent) / 100;
}

CAmount GuestShare(CAmount pot, size_t nReady)
{
    if (pot <= 0 || nReady == 0)
        return 0;
    return pot / (CAmount)nReady;
}

bool ResolveHolder(const std::string& handleIn, std::string& assetOut,
                   std::string& addressOut, std::string& err)
{
    assetOut.clear();
    addressOut.clear();
    err.clear();
    std::string handle;
    if (!lottery::NormalizeXHandle(handleIn, handle, err))
        return false;
    if (!CheckIfXAccountAssigned(handle, &assetOut)) {
        err = strprintf("@%s has not claimed a root asset", handle);
        return false;
    }
    if (!AssetIndexReady()) {
        err = "asset index is not ready";
        return false;
    }

    auto pick = [](const std::string& name, std::string& addr) -> bool {
        std::vector<std::pair<std::string, CAmount> > rows;
        int total = 0;
        if (!passetsdb->AssetAddressDir(rows, total, false, name, 32, 0))
            return false;
        CAmount best = 0;
        bool found = false;
        for (const auto& row : rows) {
            if (row.second <= 0 || row.first.empty())
                continue;
            if (!found || row.second > best) {
                best = row.second;
                addr = row.first;
                found = true;
            }
        }
        return found;
    };

    if (pick(assetOut + "!", addressOut))
        return true;
    if (pick(assetOut, addressOut))
        return true;
    err = strprintf("no holder address for %s", assetOut);
    return false;
}

std::vector<GuestInfo> ListGuests()
{
    std::vector<std::string> handles;
    {
        LOCK(cs_hostshare);
        handles = g_guests;
    }
    std::vector<GuestInfo> out;
    out.reserve(handles.size());
    for (const auto& h : handles) {
        GuestInfo g;
        g.handle = h;
        std::string err;
        if (ResolveHolder(h, g.asset, g.address, err))
            g.ready = !g.address.empty();
        else if (CheckIfXAccountAssigned(h, &g.asset))
            g.ready = false;
        out.push_back(g);
    }
    return out;
}

std::vector<std::string> GuestHandles()
{
    LOCK(cs_hostshare);
    return g_guests;
}

static bool ConfHasAssetIndex(const fs::path& path)
{
    FILE* f = fsbridge::fopen(path, "r");
    if (!f)
        return false;
    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        const size_t hash = s.find('#');
        if (hash != std::string::npos)
            s = s.substr(0, hash);
        if (s.find("assetindex") == std::string::npos)
            continue;
        if (s.find('1') != std::string::npos)
            found = true;
    }
    fclose(f);
    return found;
}

static bool AppendAssetIndexLine(const fs::path& path)
{
    if (ConfHasAssetIndex(path))
        return true;
    FILE* f = fsbridge::fopen(path, "a");
    if (!f)
        return false;
    const int n = fprintf(f, "\n# Host/guest share (listaddressesbyasset)\nassetindex=1\n");
    fclose(f);
    return n > 0;
}

bool EnableAssetIndex(std::string& err, bool& needsRestart)
{
    err.clear();
    needsRestart = false;
    if (fAssetIndex) {
        return true;
    }
    const fs::path dataConf = GetDataDir() / "xcoin.conf";
    if (!AppendAssetIndexLine(dataConf)) {
        err = "Could not write assetindex=1 to the data directory xcoin.conf";
        return false;
    }
    try {
        const fs::path mainConf = GetConfigFile(gArgs.GetArg("-conf", RAVEN_CONF_FILENAME));
        if (mainConf != dataConf)
            AppendAssetIndexLine(mainConf);
    } catch (...) {
    }
    FILE* m = fsbridge::fopen(ReindexMarkerPath(), "w");
    if (m) {
        fprintf(m, "assetindex\n");
        fclose(m);
    }
    needsRestart = true;
    return true;
}

#ifdef ENABLE_WALLET
static bool AlreadySharedLocked(const uint256& txid)
{
    return g_shared.count(txid.GetHex()) != 0;
}

static void MarkSharedLocked(const uint256& txid)
{
    g_shared.insert(txid.GetHex());
    PersistLocked();
}

void MaybeShareMatureWins()
{
    if (!Enabled())
        return;
    if (!xsession::SessionIsXVerified())
        return;
    if (!AssetIndexReady())
        return;
    if (vpwallets.empty() || !vpwallets[0])
        return;
    CWallet* pwallet = vpwallets[0];
    if (pwallet->IsLocked())
        return;

    const CScript local = lottery::GetRegistry().LocalScript();
    if (local.empty())
        return;

    const int percent = Percent();
    std::vector<GuestInfo> guests = ListGuests();
    std::vector<CTxDestination> dests;
    dests.reserve(guests.size());
    for (const auto& g : guests) {
        if (!g.ready)
            continue;
        const CTxDestination dest = DecodeDestination(g.address);
        if (!IsValidDestination(dest))
            continue;
        dests.push_back(dest);
    }

    struct Job {
        uint256 txid;
        CAmount nValue;
        COutPoint coin;
    };
    std::vector<Job> jobs;

    {
        LOCK2(cs_main, pwallet->cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = pwallet->mapWallet.begin();
             it != pwallet->mapWallet.end(); ++it) {
            const CWalletTx& wtx = it->second;
            if (!wtx.IsCoinBase())
                continue;
            if (wtx.GetBlocksToMaturity() > 0)
                continue;
            if (wtx.GetDepthInMainChain() < 1)
                continue;
            {
                LOCK(cs_hostshare);
                if (AlreadySharedLocked(wtx.GetHash()))
                    continue;
            }
            CAmount nValue = 0;
            int nOut = -1;
            for (size_t i = 0; i < wtx.tx->vout.size(); i++) {
                const CTxOut& out = wtx.tx->vout[i];
                if (nOut < 0 && out.scriptPubKey == local && (pwallet->IsMine(out) & ISMINE_SPENDABLE)) {
                    nValue = out.nValue;
                    nOut = (int)i;
                }
            }
            Job job;
            job.txid = wtx.GetHash();
            job.nValue = nValue;
            if (nOut >= 0)
                job.coin = COutPoint(wtx.GetHash(), (unsigned)nOut);
            jobs.push_back(job);
        }
    }

    for (const Job& job : jobs) {
        if (job.nValue <= 0 || job.coin.IsNull()) {
            LOCK(cs_hostshare);
            MarkSharedLocked(job.txid);
            continue;
        }
        if (dests.empty()) {
            LOCK(cs_hostshare);
            MarkSharedLocked(job.txid);
            continue;
        }
        const CAmount pot = GuestPot(job.nValue, percent);
        const CAmount each = GuestShare(pot, dests.size());
        if (each <= 0) {
            LOCK(cs_hostshare);
            MarkSharedLocked(job.txid);
            continue;
        }
        const CAmount dust = GetDustThreshold(CTxOut(each, GetScriptForDestination(dests[0])), ::dustRelayFee);
        if (each < dust) {
            LOCK(cs_hostshare);
            MarkSharedLocked(job.txid);
            continue;
        }

        std::vector<CRecipient> vecSend;
        for (const auto& dest : dests) {
            CRecipient r;
            r.scriptPubKey = GetScriptForDestination(dest);
            r.nAmount = each;
            r.fSubtractFeeFromAmount = false;
            vecSend.push_back(r);
        }

        CCoinControl ctrl;
        ctrl.Select(job.coin);
        CReserveKey reservekey(pwallet);
        CWalletTx wtxNew;
        CAmount nFeeRet = 0;
        int nChangePos = -1;
        std::string fail;
        {
            LOCK2(cs_main, pwallet->cs_wallet);
            if (!pwallet->CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, nChangePos, fail, ctrl)) {
                LogPrintf("hostshare: create failed for %s: %s\n", job.txid.GetHex(), fail);
                continue;
            }
            CValidationState state;
            if (!pwallet->CommitTransaction(wtxNew, reservekey, g_connman.get(), state)) {
                LogPrintf("hostshare: commit failed for %s: %s\n", job.txid.GetHex(), state.GetRejectReason());
                continue;
            }
        }
        LOCK(cs_hostshare);
        MarkSharedLocked(job.txid);
        LogPrintf("hostshare: shared %s (%d%% pot %s to %u guests)\n",
                  job.txid.GetHex(), percent, FormatMoney(pot), (unsigned)dests.size());
    }
}
#endif

} // namespace hostshare
