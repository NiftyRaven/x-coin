// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_raven.h>
#include <xrelease.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(xrelease_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(parse_version_tags)
{
    int v = 0;
    BOOST_CHECK(xrelease::ParseVersion("v1.0.11", v));
    BOOST_CHECK_EQUAL(v, 1001100); // 1000000*1 + 10000*0 + 100*11
    BOOST_CHECK(xrelease::ParseVersion("1.0.10", v));
    BOOST_CHECK_EQUAL(v, 1001000);
    BOOST_CHECK(xrelease::ParseVersion("v1.0.11.0-gabc", v));
    BOOST_CHECK_EQUAL(v, 1001100);
    BOOST_CHECK(xrelease::ParseVersion("2.0.0", v));
    BOOST_CHECK_EQUAL(v, 2000000);
    BOOST_CHECK(!xrelease::ParseVersion("", v));
    BOOST_CHECK(!xrelease::ParseVersion("nope", v));
    BOOST_CHECK(!xrelease::ParseVersion("v", v));
}

BOOST_AUTO_TEST_CASE(running_matches_configure)
{
    BOOST_CHECK_EQUAL(xrelease::RunningVersionString(), "1.0.12");
    BOOST_CHECK_EQUAL(xrelease::RunningTag(), "v1.0.12");
    BOOST_CHECK_EQUAL(xrelease::RunningVersion(), 1001200);
    BOOST_CHECK(xrelease::BundledNotes().find("Sign in with X stays signed in") != std::string::npos);
    BOOST_CHECK(xrelease::BundledNotes().find("What's new") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(parse_github_array_picks_newer)
{
    const std::string json =
        "["
        "{\"tag_name\":\"v1.0.12\",\"name\":\"X Coin 1.0.12\",\"body\":\"Fixed send.\","
        "\"html_url\":\"https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.12\","
        "\"draft\":false,\"prerelease\":false},"
        "{\"tag_name\":\"v1.0.11\",\"name\":\"X Coin 1.0.11\",\"body\":\"What's new in the wallet.\","
        "\"html_url\":\"https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.11\","
        "\"draft\":false,\"prerelease\":false},"
        "{\"tag_name\":\"v1.0.10\",\"name\":\"X Coin 1.0.10\",\"body\":\"Session while open.\","
        "\"html_url\":\"https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.10\","
        "\"draft\":false,\"prerelease\":false}"
        "]";
    std::vector<xrelease::Release> all;
    std::string err;
    BOOST_CHECK(xrelease::ParseReleaseFeed(json, all, err));
    BOOST_CHECK_EQUAL(all.size(), 3);

    xrelease::Release newer;
    BOOST_CHECK(xrelease::LatestNewer(all, newer, 1001100));
    BOOST_CHECK_EQUAL(newer.tag, "v1.0.12");
    BOOST_CHECK(newer.body.find("Fixed send") != std::string::npos);

    xrelease::Release none;
    BOOST_CHECK(!xrelease::LatestNewer(all, none, 1001200));

    xrelease::Release me;
    BOOST_CHECK(xrelease::FindByTag(all, "v1.0.11", me));
    BOOST_CHECK(me.body.find("What's new") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(skips_draft_and_prerelease)
{
    const std::string json =
        "["
        "{\"tag_name\":\"v1.0.99\",\"name\":\"draft\",\"body\":\"nope\",\"draft\":true,\"prerelease\":false},"
        "{\"tag_name\":\"v1.0.98\",\"name\":\"pre\",\"body\":\"nope\",\"draft\":false,\"prerelease\":true},"
        "{\"tag_name\":\"v1.0.12\",\"name\":\"real\",\"body\":\"yes\",\"draft\":false,\"prerelease\":false}"
        "]";
    std::vector<xrelease::Release> all;
    std::string err;
    BOOST_CHECK(xrelease::ParseReleaseFeed(json, all, err));
    xrelease::Release newer;
    BOOST_CHECK(xrelease::LatestNewer(all, newer, 1001100));
    BOOST_CHECK_EQUAL(newer.tag, "v1.0.12");
}

BOOST_AUTO_TEST_CASE(single_object_and_wrapped_and_not_found)
{
    std::vector<xrelease::Release> all;
    std::string err;
    BOOST_CHECK(xrelease::ParseReleaseFeed(
        "{\"tag\":\"v1.0.20\",\"notes\":\"wrap\",\"url\":\"https://example\"}", all, err));
    BOOST_CHECK_EQUAL(all.size(), 1);
    BOOST_CHECK_EQUAL(all[0].body, "wrap");

    BOOST_CHECK(xrelease::ParseReleaseFeed(
        "{\"releases\":[{\"tag_name\":\"v1.0.21\",\"body\":\"inner\"}]}", all, err));
    BOOST_CHECK_EQUAL(all[0].tag, "v1.0.21");

    BOOST_CHECK(!xrelease::ParseReleaseFeed("{\"message\":\"Not Found\"}", all, err));
    BOOST_CHECK_EQUAL(err, "Not Found");
    BOOST_CHECK(!xrelease::ParseReleaseFeed("not-json", all, err));
}

BOOST_AUTO_TEST_CASE(download_url_must_be_this_repo)
{
    BOOST_CHECK(xrelease::IsSafeDownloadUrl("https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.12"));
    BOOST_CHECK(!xrelease::IsSafeDownloadUrl("https://evil.example/payload"));
    BOOST_CHECK(!xrelease::IsSafeDownloadUrl("http://github.com/NiftyRaven/x-coin/releases"));
    BOOST_CHECK(!xrelease::IsSafeDownloadUrl("https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.12\nhttps://evil"));
}

BOOST_AUTO_TEST_SUITE_END()
