// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xrelease.h"

#include "clientversion.h"
#include "sync.h"
#include "tinyformat.h"
#include "util.h"

#include <univalue.h>

#include <cctype>
#include <cstdlib>

namespace xrelease {

static CCriticalSection cs_xrelease;
static Release g_cachedNewer;
static bool g_hasCachedNewer = false;
static bool g_fetchOk = false;
static std::string g_fetchErr = "not checked";

static std::string TrimCopy(const std::string& in)
{
    size_t a = 0;
    while (a < in.size() && std::isspace(static_cast<unsigned char>(in[a])))
        a++;
    size_t b = in.size();
    while (b > a && std::isspace(static_cast<unsigned char>(in[b - 1])))
        b--;
    return in.substr(a, b - a);
}

static std::string StripLeadingV(const std::string& in)
{
    std::string s = TrimCopy(in);
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V'))
        s = s.substr(1);
    return s;
}

int RunningVersion()
{
    return CLIENT_VERSION;
}

std::string RunningVersionString()
{
    if (CLIENT_VERSION_BUILD == 0)
        return strprintf("%d.%d.%d", CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, CLIENT_VERSION_REVISION);
    return strprintf("%d.%d.%d.%d", CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR,
                     CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD);
}

std::string RunningTag()
{
    return "v" + RunningVersionString();
}

bool ParseVersion(const std::string& tagOrVersion, int& out)
{
    out = 0;
    const std::string s = StripLeadingV(tagOrVersion);
    if (s.empty())
        return false;
    int parts[4] = {0, 0, 0, 0};
    int n = 0;
    std::string cur;
    auto flush = [&]() -> bool {
        if (cur.empty())
            return n > 0;
        for (char d : cur) {
            if (d < '0' || d > '9')
                return false;
        }
        if (n >= 4)
            return false;
        const long v = std::strtol(cur.c_str(), nullptr, 10);
        if (v < 0 || v > 99)
            return false;
        parts[n++] = (int)v;
        cur.clear();
        return true;
    };
    for (size_t i = 0; i <= s.size(); ++i) {
        const bool end = (i == s.size());
        const char c = end ? '\0' : s[i];
        if (end || c == '.' || c == '-' || c == '+') {
            if (!flush())
                return false;
            if (end || c == '-' || c == '+')
                break;
            continue;
        }
        cur.push_back(c);
    }
    if (n < 2)
        return false;
    out = 1000000 * parts[0] + 10000 * parts[1] + 100 * parts[2] + parts[3];
    return true;
}

static bool ParseOne(const UniValue& obj, Release& out)
{
    out = Release();
    if (!obj.isObject())
        return false;
    if (obj.exists("draft"))
        out.draft = obj["draft"].isTrue() || (obj["draft"].isBool() && obj["draft"].get_bool());
    if (obj.exists("prerelease"))
        out.prerelease = obj["prerelease"].isTrue() || (obj["prerelease"].isBool() && obj["prerelease"].get_bool());
    if (obj.exists("tag_name"))
        out.tag = TrimCopy(obj["tag_name"].getValStr());
    else if (obj.exists("tag"))
        out.tag = TrimCopy(obj["tag"].getValStr());
    if (out.tag.empty())
        return false;
    if (obj.exists("name"))
        out.name = obj["name"].getValStr();
    if (obj.exists("body"))
        out.body = obj["body"].getValStr();
    else if (obj.exists("notes"))
        out.body = obj["notes"].getValStr();
    if (obj.exists("html_url"))
        out.htmlUrl = obj["html_url"].getValStr();
    else if (obj.exists("url"))
        out.htmlUrl = obj["url"].getValStr();
    if (!ParseVersion(out.tag, out.version))
        return false;
    return true;
}

bool ParseReleaseFeed(const std::string& json, std::vector<Release>& out, std::string& err)
{
    out.clear();
    UniValue u;
    if (!u.read(json)) {
        err = "release feed is not valid JSON";
        return false;
    }
    UniValue arr(UniValue::VARR);
    if (u.isArray()) {
        arr = u;
    } else if (u.isObject() && u.exists("releases") && u["releases"].isArray()) {
        arr = u["releases"];
    } else if (u.isObject() && (u.exists("tag_name") || u.exists("tag"))) {
        arr.push_back(u);
    } else if (u.isObject() && u.exists("message")) {
        err = u["message"].getValStr();
        if (err.empty())
            err = "release feed error";
        return false;
    } else {
        err = "release feed must be a GitHub releases array or {releases:[...]}";
        return false;
    }
    for (size_t i = 0; i < arr.size(); ++i) {
        Release r;
        if (ParseOne(arr[i], r))
            out.push_back(r);
    }
    if (out.empty()) {
        err = "no parseable releases in feed";
        return false;
    }
    err.clear();
    return true;
}

static bool IsSameEdition(const Release& r);

bool LatestNewer(const std::vector<Release>& all, Release& out, int running)
{
    bool found = false;
    out = Release();
    for (const Release& r : all) {
        if (r.draft || r.prerelease)
            continue;
        if (r.version <= running)
            continue;
        if (!IsSameEdition(r))
            continue;
        if (!found || r.version > out.version) {
            out = r;
            found = true;
        }
    }
    return found;
}

std::string WalletEdition()
{
#ifdef XCOIN_LIGHT_WALLET
    return "light";
#else
    return "heavy";
#endif
}

static bool IsSameEdition(const Release& r)
{
    const bool namedHeavy = r.name.find("Heavy") != std::string::npos;
    const bool namedLight = r.name.find("Light") != std::string::npos;
    if (WalletEdition() == "heavy") {
        if (namedLight && !namedHeavy)
            return false;
        return true;
    }
    if (namedHeavy && !namedLight)
        return false;
    return true;
}

bool FindByTag(const std::vector<Release>& all, const std::string& tag, Release& out)
{
    const std::string want = TrimCopy(tag);
    for (const Release& r : all) {
        if (r.tag == want) {
            out = r;
            return true;
        }
    }
    return false;
}

std::string BundledNotes()
{
#ifdef XCOIN_LIGHT_WALLET
    return
        "X Coin " + RunningVersionString() + " Light\n"
        "\n"
        "This is the Light wallet. Heavy is the tree-nav Market wallet.\n"
        "Same chain. Same protocol. Same data folder.\n"
        "\n"
        "- Home is buttons first: Sign in with X, lottery, Receive, Send,\n"
        "  Activity, Transfer assets, Share lottery wins.\n"
        "- Lottery shows live X Verified nodes, not the frozen draw set.\n"
        "- Connected peers name the Master Seed Node vs public nodes.\n"
        "- Help → What's new stays on Light. Heavy is a parallel zip.\n"
        "- No Market page. Heavy listings travel as xord; Light ignores them\n"
        "  and still confirms a completed buy.\n"
        "- Share lottery wins: an X Verified host can send a percent of each\n"
        "  mature lottery payout equally among invited guests. Guests sign\n"
        "  in with X and claim a root; they do not need a blue check and they\n"
        "  never enter the hat.\n"
        "- Sign in with X stays signed in until you close the wallet.\n"
        "\n"
        "Light and Heavy are parallel wallets, not a forced upgrade path.\n"
        "Download from GitHub Releases (not Code → Download ZIP).\n";
#else
    return
        "X Coin " + RunningVersionString() + " Heavy\n"
        "\n"
        "This is the Heavy wallet. Light is the simple Home wallet.\n"
        "Same chain. Same protocol. Same data folder.\n"
        "\n"
        "- Sync / loading uses the dark wallet theme (not a white flash).\n"
        "- Advanced menu items are one size. Home shows Unlock when the\n"
        "  wallet is locked: pick a duration, then the passphrase. That is\n"
        "  walletpassphrase with a timeout. Locking stops lottery heartbeat;\n"
        "  unlocking restores it.\n"
        "- Home and Help → About show this wallet version.\n"
        "- Send clears the address and amount after a successful send.\n"
        "- Packaged xcoin.conf sets assetindex=1 (holder lookup for lottery\n"
        "  share). It does not turn on txindex or force an archival node.\n"
        "  A datadir that already ran without the index needs a one-time\n"
        "  reindex. New wallets index from the first block.\n"
        "- Lottery header and Home show live X Verified nodes, not the\n"
        "  frozen draw set. Winners still use the slot freeze.\n"
        "- Market (Assets → Market): list a bag for XFER. Other Heavy\n"
        "  wallets buy it. No copy-paste. Listings travel peer to peer.\n"
        "- Tree nav: WALLET, ASSETS, REWARDS, NODE, ADVANCED.\n"
        "- Sign in with X stays signed in until you close the wallet.\n"
        "- Sign in with X Client ID was rotated after the prior X app\n"
        "  was suspended. Upgrade this zip to restore Sign in with X.\n"
        "\n"
        "Stay on Light: download the Light zip. Heavy: this zip.\n"
        "They are parallel wallets, not a forced upgrade path.\n"
        "Download from GitHub Releases (not Code → Download ZIP).\n"
        "Windows: X-Coin-" + RunningVersionString() + "-Windows.zip → X Coin Wallet.exe\n"
        "Linux: X-Coin-" + RunningVersionString() + "-Linux-x86_64.tar.gz → X Coin Wallet\n";
#endif
}

std::string BundledName()
{
#ifdef XCOIN_LIGHT_WALLET
    return "X Coin " + RunningVersionString() + " Light — Home wallet";
#else
    return "X Coin " + RunningVersionString() + " Heavy — Market and tree nav";
#endif
}

std::string BundledHtmlUrl()
{
#ifdef XCOIN_LIGHT_WALLET
    return "https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light";
#else
    return "https://github.com/NiftyRaven/x-coin/releases/tag/v" + RunningVersionString();
#endif
}

Release BundledRelease()
{
    Release r;
    r.tag = RunningTag();
    r.name = BundledName();
    r.body = BundledNotes();
    r.htmlUrl = BundledHtmlUrl();
    r.version = RunningVersion();
    return r;
}

std::string DefaultFeedUrl()
{
    return "https://api.github.com/repos/NiftyRaven/x-coin/releases?per_page=15";
}

std::string FeedUrl()
{
    const std::string u = TrimCopy(gArgs.GetArg("-xreleaseurl", ""));
    if (!u.empty())
        return u;
    return DefaultFeedUrl();
}

bool CheckEnabled()
{
    return !gArgs.GetBoolArg("-nocheckupdates", false);
}

void SetCachedNewer(const Release& r)
{
    LOCK(cs_xrelease);
    g_cachedNewer = r;
    g_hasCachedNewer = true;
}

bool GetCachedNewer(Release& out)
{
    LOCK(cs_xrelease);
    if (!g_hasCachedNewer)
        return false;
    out = g_cachedNewer;
    return true;
}

void ClearCachedNewer()
{
    LOCK(cs_xrelease);
    g_cachedNewer = Release();
    g_hasCachedNewer = false;
}

void SetFetchStatus(bool ok, const std::string& err)
{
    LOCK(cs_xrelease);
    g_fetchOk = ok;
    g_fetchErr = err;
}

bool LastFetchOk()
{
    LOCK(cs_xrelease);
    return g_fetchOk;
}

std::string LastFetchError()
{
    LOCK(cs_xrelease);
    return g_fetchErr;
}

bool IsSafeDownloadUrl(const std::string& url)
{
    const std::string u = TrimCopy(url);
    static const char* pfx = "https://github.com/NiftyRaven/x-coin/";
    if (u.rfind(pfx, 0) != 0)
        return false;
    for (unsigned char c : u) {
        if (c <= 32 || c == '\\' || c == '"' || c == '\'' )
            return false;
    }
    return true;
}

UniValue ToUniValue(const Release& r)
{
    UniValue o(UniValue::VOBJ);
    o.push_back(Pair("tag", r.tag));
    o.push_back(Pair("name", r.name));
    o.push_back(Pair("notes", r.body));
    o.push_back(Pair("url", r.htmlUrl));
    o.push_back(Pair("version", r.version));
    o.push_back(Pair("draft", r.draft));
    o.push_back(Pair("prerelease", r.prerelease));
    return o;
}

} // namespace xrelease
