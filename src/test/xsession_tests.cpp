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
    xsession::UsersMe me;
    std::string err;
    BOOST_CHECK(xsession::ParseUsersMe(
        "{\"data\":{\"id\":\"42\",\"username\":\"NFTRVN\"}}", me, err));
    BOOST_CHECK_EQUAL(me.userId, "42");
    BOOST_CHECK_EQUAL(me.username, "nftrvn");
    BOOST_CHECK(!me.verified);
    BOOST_CHECK(!xsession::ParseUsersMe("{}", me, err));
    BOOST_CHECK(!xsession::ParseUsersMe("not-json", me, err));
}

BOOST_AUTO_TEST_CASE(parse_users_me_verified_fields)
{
    xsession::UsersMe me;
    std::string err;
    BOOST_CHECK(xsession::ParseUsersMe(
        "{\"data\":{\"id\":\"1\",\"username\":\"alice\",\"verified\":true,\"verified_type\":\"blue\"}}",
        me, err));
    BOOST_CHECK(me.verified);
    BOOST_CHECK_EQUAL(me.verifiedType, "blue");
    BOOST_CHECK(xsession::IsOfficialXVerified(me.verified, me.verifiedType));

    BOOST_CHECK(xsession::ParseUsersMe(
        "{\"data\":{\"id\":\"2\",\"username\":\"gov\",\"verified\":false,\"verified_type\":\"government\"}}",
        me, err));
    BOOST_CHECK(me.verified);
    BOOST_CHECK_EQUAL(me.verifiedType, "government");

    BOOST_CHECK(xsession::ParseUsersMe(
        "{\"data\":{\"id\":\"3\",\"username\":\"ghost\",\"verified\":false}}",
        me, err));
    BOOST_CHECK(!me.verified);
    BOOST_CHECK(!xsession::IsOfficialXVerified(false, ""));
}

BOOST_AUTO_TEST_CASE(typed_handle_rejected_session_accepted)
{
    xsession::ClearSession();
    std::string err;
    BOOST_CHECK(!xsession::RequireSession(err));
    BOOST_CHECK(err.find("Sign in with X") != std::string::npos);
    BOOST_CHECK(!xsession::RequireHandle("nftrvn", err));
    BOOST_CHECK(err.find("Sign in with X") != std::string::npos);

    BOOST_CHECK(xsession::ApplyUsersMePayload(
        "{\"data\":{\"id\":\"99\",\"username\":\"alice\"}}", err, nullptr));
    BOOST_CHECK_EQUAL(xsession::SignedInHandle(), "alice");
    BOOST_CHECK_EQUAL(xsession::SignedInUserId(), "99");
    BOOST_CHECK(!xsession::SessionIsXVerified());

    BOOST_CHECK(!xsession::RequireHandle("nftrvn", err));
    BOOST_CHECK(err.find("impersonation") != std::string::npos || err.find("cannot use") != std::string::npos);

    BOOST_CHECK(xsession::RequireSession(err));
    BOOST_CHECK(xsession::RequireHandle("alice", err));
    BOOST_CHECK(xsession::RequireHandle("ALICE", err));
    BOOST_CHECK(!xsession::RequireXVerified(err));
}

BOOST_AUTO_TEST_CASE(mock_verified_compact_and_nftrvn_default)
{
    xsession::ClearSession();
    std::string err;
    BOOST_CHECK(xsession::ApplyUsersMePayload("ghost", err, nullptr));
    BOOST_CHECK(xsession::SessionIsXVerified());

    BOOST_CHECK(xsession::ApplyUsersMePayload("ghost:verified", err, nullptr));
    BOOST_CHECK(xsession::SessionIsXVerified());
    BOOST_CHECK_EQUAL(xsession::SignedInVerifiedType(), "blue");

    BOOST_CHECK(xsession::ApplyUsersMePayload("alice:99:unverified", err, nullptr));
    BOOST_CHECK_EQUAL(xsession::SignedInHandle(), "alice");
    BOOST_CHECK_EQUAL(xsession::SignedInUserId(), "99");
    BOOST_CHECK(!xsession::SessionIsXVerified());

    BOOST_CHECK(xsession::ApplyUsersMePayload("NFTRVN", err, nullptr));
    BOOST_CHECK_EQUAL(xsession::SignedInHandle(), "nftrvn");
    BOOST_CHECK(xsession::SessionIsXVerified());

    BOOST_CHECK(xsession::ApplyUsersMePayload("NFTRVN:unverified", err, nullptr));
    BOOST_CHECK(!xsession::SessionIsXVerified());
}

BOOST_AUTO_TEST_CASE(proof_binds_id_username_and_verified)
{
    xsession::ClearSession();
    std::string err;
    BOOST_CHECK(xsession::SaveSession("1001", "bob", 2000000000, err, false, ""));
    xsession::Session s;
    BOOST_CHECK(xsession::LoadSession(s, err));
    BOOST_CHECK_EQUAL(s.userId, "1001");
    BOOST_CHECK_EQUAL(s.username, "bob");
    BOOST_CHECK(!s.verified);
    BOOST_CHECK(!s.proofHex.empty());
    BOOST_CHECK_EQUAL(xsession::ComputeProof("1001", "bob", 2000000000, false, ""), s.proofHex);

    BOOST_CHECK(xsession::SaveSession("1001", "bob", 2000000000, err, true, "blue"));
    BOOST_CHECK(xsession::LoadSession(s, err));
    BOOST_CHECK(s.verified);
    BOOST_CHECK_EQUAL(s.verifiedType, "blue");
    BOOST_CHECK(s.IsXVerified());
    BOOST_CHECK(xsession::ComputeProof("1001", "bob", 2000000000, false, "") != s.proofHex);
}

BOOST_AUTO_TEST_SUITE_END()
