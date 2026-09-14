// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_raven.h>
#include <hostshare.h>
#include <amount.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(hostshare_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(guest_pot_is_percent_of_host_output)
{
    const CAmount win1250 = 1250 * COIN;
    BOOST_CHECK_EQUAL(hostshare::GuestPot(win1250, 20), 250 * COIN);
    BOOST_CHECK_EQUAL(hostshare::GuestShare(250 * COIN, 1), 250 * COIN);
    BOOST_CHECK_EQUAL(hostshare::GuestShare(250 * COIN, 2), 125 * COIN);

    const CAmount win5000 = 5000 * COIN;
    BOOST_CHECK_EQUAL(hostshare::GuestPot(win5000, 20), 1000 * COIN);
    BOOST_CHECK_EQUAL(hostshare::GuestShare(hostshare::GuestPot(win5000, 20), 2), 500 * COIN);

    BOOST_CHECK_EQUAL(hostshare::GuestPot(0, 20), 0);
    BOOST_CHECK_EQUAL(hostshare::GuestPot(win1250, 0), 0);
    BOOST_CHECK_EQUAL(hostshare::GuestShare(250 * COIN, 0), 0);
    BOOST_CHECK_EQUAL(hostshare::GuestPot(win1250, 100), win1250);
}

BOOST_AUTO_TEST_CASE(hat_slice_not_full_subsidy)
{
    // After first halving: 2500 split to 2 winners = 1250. Guests get % of 1250.
    const CAmount hostSlice = 1250 * COIN;
    const CAmount pot = hostshare::GuestPot(hostSlice, 20);
    BOOST_CHECK_EQUAL(pot, 250 * COIN);
    BOOST_CHECK(pot != hostshare::GuestPot(2500 * COIN, 20));
    BOOST_CHECK(pot != hostshare::GuestPot(5000 * COIN, 20));
}

BOOST_AUTO_TEST_CASE(percent_and_enable_guards)
{
    hostshare::ResetForTest();
    std::string err;
    BOOST_CHECK(!hostshare::SetPercent(0, err));
    BOOST_CHECK(!hostshare::SetPercent(101, err));
    BOOST_CHECK(hostshare::SetPercent(20, err));
    BOOST_CHECK_EQUAL(hostshare::Percent(), 20);
    BOOST_CHECK(!hostshare::SetEnabled(true, err));
    BOOST_CHECK(err.find("X Verified") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(invite_rejects_missing_root)
{
    hostshare::ResetForTest();
    std::string err;
    BOOST_CHECK(!hostshare::InviteGuest("ghost", err));
    BOOST_CHECK(err.find("root") != std::string::npos || err.find("Verified") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
