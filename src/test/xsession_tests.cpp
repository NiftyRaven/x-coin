// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_raven.h>
#include <datadir_choice.h>
#include <xsession.h>
#include <xoauth_cooldown.h>
#include <xoauth_token.h>
#include <lottery.h>
#include <chainparamsbase.h>
#include <utiltime.h>

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
    BOOST_CHECK(err.find("claim") != std::string::npos);
    BOOST_CHECK(err.find("send, receive") == std::string::npos);
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

BOOST_AUTO_TEST_CASE(wall_clock_exp_does_not_kick_live_session)
{
    xsession::ClearSession();
    std::string err;
    xsession::Session s;

    // Past exp (the old 2h leftover / mocktime-past-exp case) must still
    // load. File + valid HMAC is the session; ClearSession is the only kick.
    BOOST_CHECK(xsession::SaveSession("1", "bob", 1, err, false, ""));
    SetMockTime(100000);
    BOOST_CHECK(xsession::LoadSession(s, err));
    BOOST_CHECK_EQUAL(s.username, "bob");
    BOOST_CHECK_EQUAL(s.expiresAt, 1);

    BOOST_CHECK(xsession::SaveSession("1", "bob", 0, err, false, ""));
    BOOST_CHECK(xsession::LoadSession(s, err));
    BOOST_CHECK_EQUAL(s.expiresAt, 0);

    BOOST_CHECK(xsession::SaveSession("1", "bob", 4102444800LL, err, false, ""));
    SetMockTime(4102444801LL);
    BOOST_CHECK(xsession::LoadSession(s, err));
    BOOST_CHECK_EQUAL(s.expiresAt, 4102444800LL);
    BOOST_CHECK(xsession::HasValidSession());
    BOOST_CHECK_EQUAL(xsession::SignedInHandle(), "bob");

    SetMockTime(0);
    xsession::ClearSession();
    BOOST_CHECK(!xsession::LoadSession(s, err));
    BOOST_CHECK(!xsession::HasValidSession());
}

BOOST_AUTO_TEST_CASE(oauth_cooldown_is_one_minute_login_wait_is_five)
{
    BOOST_CHECK_EQUAL(XOAUTH_COOLDOWN_MS, 60 * 1000);
    BOOST_CHECK_EQUAL(XOAUTH_LOGIN_WAIT_MS, 5 * 60 * 1000);
    BOOST_CHECK_EQUAL(XOAuthCooldownRemainingMs(1000, 0), 0);
    BOOST_CHECK_EQUAL(XOAuthCooldownRemainingMs(1000, 2000), 0);
    BOOST_CHECK_EQUAL(XOAuthCooldownRemainingMs(5000, 1000, 10000), 6000);
    BOOST_CHECK_EQUAL(XOAuthCooldownRemainingMs(11000, 1000, 10000), 0);
    BOOST_CHECK_EQUAL(XOAuthCooldownRemainingMs(1000 + 59999, 1000), 1);
    BOOST_CHECK_EQUAL(XOAuthCooldownRemainingMs(1000 + 60000, 1000), 0);
}

BOOST_AUTO_TEST_CASE(bearer_survives_async_users_me)
{
    xsession::ClearSession();
    XOAuthUsersMeHandoff handoff;
    handoff.onTokenResponse("live-token");
    // users/me is already in flight with this header. The hold must still match.
    BOOST_CHECK_EQUAL(handoff.sentToUsersMe, "live-token");
    BOOST_CHECK(handoff.holdSurvivesUsersMe());

    std::string bearer;
    std::string err;
    BOOST_CHECK(XOAuthBearerAfterUsersMe(handoff, bearer, err));
    BOOST_CHECK_EQUAL(bearer, "live-token");
    xsession::SetClaimBearer(bearer);
    BOOST_CHECK_EQUAL(xsession::ClaimBearer(), "live-token");

    BOOST_CHECK(xsession::SaveSession("42", "the_xhrist", 0, err, false, ""));
    xsession::Session s;
    BOOST_CHECK(xsession::LoadSession(s, err));
    BOOST_CHECK_EQUAL(s.username, "the_xhrist");
    // SaveSession + LoadSession must not drop the in-memory claim token.
    BOOST_CHECK_EQUAL(xsession::ClaimBearer(), "live-token");

    // The 1.0.24 bug cleared the member before users/me returned.
    handoff.clear();
    BOOST_CHECK(!handoff.holdSurvivesUsersMe());
    BOOST_CHECK(!XOAuthBearerAfterUsersMe(handoff, bearer, err));
    BOOST_CHECK(err.find("access token") != std::string::npos);
    BOOST_CHECK(bearer.empty());

    xsession::ClearSession();
    BOOST_CHECK(xsession::ClaimBearer().empty());
    BOOST_CHECK(!xsession::HasValidSession());
}

BOOST_AUTO_TEST_CASE(saved_datadir_wins_without_command_line)
{
    const std::string osDefault = "C:\\Users\\a\\AppData\\Roaming\\XCoin";
    const std::string saved = "D:\\testxcoin";

    const DataDirChoice normal = ChooseStartupDataDir(false, "", saved, osDefault);
    BOOST_CHECK(!normal.commandLineWins);
    BOOST_CHECK(normal.setDataDirArg);
    BOOST_CHECK_EQUAL(normal.path, saved);

    const DataDirChoice cmdline = ChooseStartupDataDir(true, saved, osDefault, osDefault);
    BOOST_CHECK(cmdline.commandLineWins);
    BOOST_CHECK(!cmdline.setDataDirArg);
    BOOST_CHECK_EQUAL(cmdline.path, saved);

    const DataDirChoice unset = ChooseStartupDataDir(false, "", "", osDefault);
    BOOST_CHECK(!unset.commandLineWins);
    BOOST_CHECK(!unset.setDataDirArg);
    BOOST_CHECK_EQUAL(unset.path, osDefault);

    const DataDirChoice alreadyDefault = ChooseStartupDataDir(false, "", osDefault, osDefault);
    BOOST_CHECK(!alreadyDefault.setDataDirArg);
    BOOST_CHECK_EQUAL(alreadyDefault.path, osDefault);
}

BOOST_AUTO_TEST_SUITE_END()
