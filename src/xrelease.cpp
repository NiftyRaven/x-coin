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

bool LatestNewer(const std::vector<Release>& all, Release& out, int running)
{
    bool found = false;
    out = Release();
    for (const Release& r : all) {
        if (r.draft || r.prerelease)
            continue;
        if (r.version <= running)
            continue;
        if (!found || r.version > out.version) {
            out = r;
            found = true;
        }
    }
    return found;
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
    return
        "X Coin 1.0.11\n"
        "\n"
        "What changed in this wallet\n"
        "\n"
        "- Sign in with X stays signed in until you close the wallet. There is\n"
        "  no 2-hour (or any wall-clock) kick while X Coin Wallet is open.\n"
        "  File → Exit or closing the window signs you out. Headless xcoind\n"
        "  keeps the session across restart.\n"
        "- What's new lives in the wallet. Help → What's new shows these notes.\n"
        "  When a newer GitHub Release is published and the feed is reachable,\n"
        "  Home and the header show that version plus its release notes.\n"
        "  You do not have to leave the wallet to read them.\n"
        "- Assets tab is still sub + unique only. The free MAIN/root is Claim\n"
        "  after Sign-in. Send and receive work without Sign-in. Lottery still\n"
        "  needs X Verified (blue check).\n"
        "\n"
        "Download later wallets from GitHub Releases (not Code → Download ZIP).\n"
        "Windows: X-Coin-1.0.11-Windows.zip → X Coin Wallet.exe\n"
        "Linux: X-Coin-1.0.11-Linux-x86_64.tar.gz → X Coin Wallet\n";
}

std::string BundledName()
{
    return "X Coin 1.0.11 — session until quit; What's new in the wallet";
}

std::string BundledHtmlUrl()
{
    return "https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.11";
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
