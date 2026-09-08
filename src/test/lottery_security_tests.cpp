// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_raven.h>
#include <lottery.h>
#include <key.h>
#include <script/standard.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(lottery_security_tests, TestingSetup)

static CScript P2PKHFromKey(const CKey& key)
{
    return GetScriptForDestination(CKeyID(key.GetPubKey().GetID()));
}

BOOST_AUTO_TEST_CASE(allowlist_requires_handle_not_userid_alone)
{
    lottery::GetAllowlist().Reset();
    std::string err;
    BOOST_CHECK(lottery::GetAllowlist().Add("alice", 99, err, false));
    BOOST_CHECK(lottery::GetAllowlist().Contains("alice", 99));
    BOOST_CHECK(lottery::GetAllowlist().Contains("alice", 0));
    BOOST_CHECK(!lottery::GetAllowlist().Contains("alice", 100));
    BOOST_CHECK(!lottery::GetAllowlist().Contains("eve", 99));
    BOOST_CHECK(!lottery::GetAllowlist().Contains("eve", 0));
}

BOOST_AUTO_TEST_CASE(heartbeat_signature_binds_payout_key)
{
    CKey key;
    key.MakeNewKey(true);
    const CScript script = P2PKHFromKey(key);
    const int64_t t = 1'700'000'000;
    std::vector<unsigned char> sig;
    BOOST_CHECK(lottery::SignHeartbeat(key, t, script, "alice", 99, sig));
    BOOST_CHECK(lottery::VerifyHeartbeatSig(script, t, "alice", 99, sig));
    BOOST_CHECK(lottery::VerifyHeartbeatSig(script, t, "ALICE", 99, sig));
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(script, t, "alice", 98, sig));
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(script, t + 1, "alice", 99, sig));
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(script, t, "bob", 99, sig));
    std::vector<unsigned char> empty;
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(script, t, "alice", 99, empty));

    CKey other;
    other.MakeNewKey(true);
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(P2PKHFromKey(other), t, "alice", 99, sig));
}

BOOST_AUTO_TEST_CASE(live_handle_cannot_be_stolen_by_another_script)
{
    lottery::GetAllowlist().Reset();
    lottery::GetRegistry().Reset();
    std::string err;
    BOOST_CHECK(lottery::GetAllowlist().Add("alice", 0, err, false));
    BOOST_CHECK(lottery::GetAllowlist().Add("eve", 0, err, false));

    CKey aliceKey;
    aliceKey.MakeNewKey(true);
    const CScript aliceScript = P2PKHFromKey(aliceKey);
    const int64_t now = 1'800'000'000;
    BOOST_CHECK(lottery::GetRegistry().Heartbeat(aliceScript, now, "alice", 0));

    CKey eveKey;
    eveKey.MakeNewKey(true);
    const CScript eveScript = P2PKHFromKey(eveKey);
    BOOST_CHECK(!lottery::GetRegistry().Heartbeat(eveScript, now + 1, "alice", 0));

    const auto nodes = lottery::GetRegistry().ActiveNodes(now + 1);
    BOOST_REQUIRE_EQUAL(nodes.size(), 1U);
    BOOST_CHECK(nodes[0].script == aliceScript);
    BOOST_CHECK_EQUAL(nodes[0].x.handle, "alice");

    // After TTL the handle can be rebound (first-seen residual; pin closes it).
    BOOST_CHECK(lottery::GetRegistry().Heartbeat(eveScript, now + lottery::HEARTBEAT_TTL_SECONDS + 1,
                                                 "alice", 0));
}

BOOST_AUTO_TEST_CASE(allowlist_pin_rejects_other_payout)
{
    lottery::GetAllowlist().Reset();
    lottery::GetRegistry().Reset();
    CKey aliceKey;
    aliceKey.MakeNewKey(true);
    const CScript aliceScript = P2PKHFromKey(aliceKey);
    std::string err;
    BOOST_CHECK(lottery::GetAllowlist().Add("alice", 0, err, false, aliceScript));
    BOOST_CHECK(lottery::GetAllowlist().PinnedScript("alice") == aliceScript);

    CKey eveKey;
    eveKey.MakeNewKey(true);
    BOOST_CHECK(!lottery::GetRegistry().Heartbeat(P2PKHFromKey(eveKey), 10, "alice", 0));
    BOOST_CHECK(lottery::GetRegistry().Heartbeat(aliceScript, 10, "alice", 0));
}

BOOST_AUTO_TEST_CASE(unlisted_heartbeat_ignored)
{
    lottery::GetAllowlist().Reset();
    lottery::GetRegistry().Reset();
    CKey key;
    key.MakeNewKey(true);
    BOOST_CHECK(!lottery::GetRegistry().Heartbeat(P2PKHFromKey(key), 10, "ghost", 0));
    BOOST_CHECK_EQUAL(lottery::GetRegistry().Count(10), 0U);
}

BOOST_AUTO_TEST_SUITE_END()
