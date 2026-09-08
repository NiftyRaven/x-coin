// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <assets/assets.h>
#include <assets/xaccount.h>
#include <lottery.h>
#include <test/test_raven.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(xaccount_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(handle_to_root_mapping)
{
    std::string name, err;

    BOOST_CHECK(MapXHandleToRootName("NFTRVN", name, err));
    BOOST_CHECK_EQUAL(name, "NFTRVN");

    BOOST_CHECK(MapXHandleToRootName("nftrvn", name, err));
    BOOST_CHECK_EQUAL(name, "NFTRVN");

    BOOST_CHECK(MapXHandleToRootName("ab", name, err));
    BOOST_CHECK_EQUAL(name, "XAB");

    BOOST_CHECK(MapXHandleToRootName("a", name, err));
    BOOST_CHECK_EQUAL(name, "XXA");

    // Explicit edge rule: leading/trailing `_` become `X` (asset names
    // cannot start or end with punctuation). Not a silent strip.
    BOOST_CHECK(MapXHandleToRootName("_alice", name, err));
    BOOST_CHECK_EQUAL(name, "XALICE");
    BOOST_CHECK(MapXHandleToRootName("alice_", name, err));
    BOOST_CHECK_EQUAL(name, "ALICEX");

    // Consecutive underscores collapse to one, then edges are fixed.
    BOOST_CHECK(MapXHandleToRootName("a__b", name, err));
    BOOST_CHECK_EQUAL(name, "A_B");

    // 26-character X handle maps 1:1. Do not truncate.
    const std::string h26 = "abcdefghijabcdefghijabcdef";
    BOOST_CHECK_EQUAL(h26.size(), 26u);
    BOOST_CHECK(MapXHandleToRootName(h26, name, err));
    BOOST_CHECK_EQUAL(name, "ABCDEFGHIJABCDEFGHIJABCDEF");
    BOOST_CHECK_EQUAL(name.size(), 26u);
    BOOST_CHECK(IsAssetNameARoot(name));

    // 32-character handle still fits the root max.
    const std::string h32(32, 'a');
    BOOST_CHECK(MapXHandleToRootName(h32, name, err));
    BOOST_CHECK_EQUAL(name.size(), 32u);
    BOOST_CHECK_EQUAL(name, std::string(32, 'A'));

    // Longer than the root max is a hard fail, never truncated.
    const std::string h33(33, 'a');
    BOOST_CHECK(!MapXHandleToRootName(h33, name, err));
    BOOST_CHECK(name.empty());
}

BOOST_AUTO_TEST_CASE(derive_26_char_handle_no_suffix)
{
    std::string name, err;
    const std::string h26 = "abcdefghijabcdefghijabcdef";
    BOOST_CHECK(DeriveMainAssetName(h26, name, err, nullptr));
    BOOST_CHECK_EQUAL(name, "ABCDEFGHIJABCDEFGHIJABCDEF");
}

BOOST_AUTO_TEST_CASE(collision_suffix_does_not_truncate)
{
    std::string name, err;
    const std::string h26 = "abcdefghijabcdefghijabcdef";
    BOOST_CHECK(MapXHandleToRootName(h26, name, err));
    // A suffix that would overflow 32 is skipped; `_2` still fits.
    BOOST_CHECK(name.size() + 2 <= (size_t)MAX_ROOT_NAME_LENGTH);
    std::string with2 = name + "_2";
    BOOST_CHECK(IsAssetNameARoot(with2));
    BOOST_CHECK_EQUAL(with2.size(), 28u);
}

BOOST_AUTO_TEST_CASE(normalize_x_handle_accepts_26)
{
    std::string out, err;
    const std::string h26 = "Abcdefghijabcdefghijabcdef";
    BOOST_CHECK(lottery::NormalizeXHandle(h26, out, err));
    BOOST_CHECK_EQUAL(out, "abcdefghijabcdefghijabcdef");
    BOOST_CHECK_EQUAL(out.size(), 26u);
}

BOOST_AUTO_TEST_SUITE_END()
