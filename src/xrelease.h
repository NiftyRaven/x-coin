// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_XRELEASE_H
#define XCOIN_XRELEASE_H

#include <string>
#include <vector>

class UniValue;

/**
 * In-wallet release notes / update notify.
 *
 * Parses GitHub Releases JSON (or a small {releases:[...]} / single-object
 * feed). The GUI fetches the feed; this module does not do HTTP.
 * A valid HMAC session is unrelated — updates are for every wallet.
 */
namespace xrelease {

struct Release {
    std::string tag;      // v1.0.11
    std::string name;
    std::string body;
    std::string htmlUrl;
    int version = 0;      // CLIENT_VERSION-style
    bool draft = false;
    bool prerelease = false;
};

/** CLIENT_VERSION of this binary. */
int RunningVersion();
/** "1.0.11" (build omitted when 0). */
std::string RunningVersionString();
/** "v1.0.11" */
std::string RunningTag();

bool ParseVersion(const std::string& tagOrVersion, int& out);

/** GitHub array, {releases:[...]}, or one release object. */
bool ParseReleaseFeed(const std::string& json, std::vector<Release>& out, std::string& err);

/** Highest published non-draft/prerelease newer than `running`. */
bool LatestNewer(const std::vector<Release>& all, Release& out, int running = RunningVersion());

bool FindByTag(const std::vector<Release>& all, const std::string& tag, Release& out);

std::string BundledNotes();
std::string BundledName();
std::string BundledHtmlUrl();
Release BundledRelease();

std::string DefaultFeedUrl();
std::string FeedUrl();
bool CheckEnabled();

void SetCachedNewer(const Release& r);
bool GetCachedNewer(Release& out);
void ClearCachedNewer();

void SetFetchStatus(bool ok, const std::string& err);
bool LastFetchOk();
std::string LastFetchError();

UniValue ToUniValue(const Release& r);

} // namespace xrelease

#endif
