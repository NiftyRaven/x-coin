// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_raven.h>
#include <xsession.h>
#include <lottery.h>
#include <chainparamsbase.h>

#include <boost/test/unit_test.hpp>

struct XSessionTestingSetup : public TestingSetup {
    XSessionTestingSetup() : TestingSetup(CBaseChainParams::REGTEST) {}
};

BOOST_FIXTURE_TEST_SUITE(xsession_tests, XSessionTestingSetup)

BOOST_AUTO_TEST_CASE(parse_users_me)
{
    std::string id, name, err;
    BOOST_CHECK(xsession::ParseUsersMe(
        "{\"data\":{\"id\":\"42\",\"username\":\"NFTRVN\"}}", id, name, err));
    BOOST_CHECK_EQUAL(id, "42");
    BOOST_CHECK_EQUAL(name, "nftrvn");
    BOOST_CHECK(!xsession::ParseUsersMe("{}", id, name, err));
    BOOST_CHECK(!xsession::ParseUsersMe("not-json", id, name, err));
}

BOOST_AUTO_TEST_CASE(typed_handle_rejected_session_accepted)
{
    xsession::ClearSession();
    std::string err;
    BOOST_CHECK(!xsession::RequireHandle("nftrvn", err));
    BOOST_CHECK(err.find("Sign in with X") != std::string::npos);

    BOOST_CHECK(xsession::ApplyUsersMePayload(
        "{\"data\":{\"id\":\"99\",\"username\":\"alice\"}}", err, nullptr));
    BOOST_CHECK_EQUAL(xsession::SignedInHandle(), "alice");
    BOOST_CHECK_EQUAL(xsession::SignedInUserId(), "99");

    BOOST_CHECK(!xsession::RequireHandle("nftrvn", err));
    BOOST_CHECK(err.find("impersonation") != std::string::npos || err.find("cannot use") != std::string::npos);

    BOOST_CHECK(xsession::RequireHandle("alice", err));
    BOOST_CHECK(xsession::RequireHandle("ALICE", err));
}

BOOST_AUTO_TEST_CASE(proof_binds_id_and_username)
{
    xsession::ClearSession();
    std::string err;
    BOOST_CHECK(xsession::SaveSession("1001", "bob", 2000000000, err));
    xsession::Session s;
    BOOST_CHECK(xsession::LoadSession(s, err));
    BOOST_CHECK_EQUAL(s.userId, "1001");
    BOOST_CHECK_EQUAL(s.username, "bob");
    BOOST_CHECK(!s.proofHex.empty());
    BOOST_CHECK_EQUAL(xsession::ComputeProof("1001", "bob", 2000000000), s.proofHex);
}

BOOST_AUTO_TEST_SUITE_END()
