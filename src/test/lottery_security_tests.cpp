// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_raven.h>
#include <lottery.h>
#include <xlookup.h>
#include <xsession.h>
#include <key.h>
#include <script/standard.h>
#include <consensus/validation.h>
#include <chain.h>
#include <chainparams.h>
#include <miner.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

static CScript P2PKHFromKey(const CKey& key)
{
    return GetScriptForDestination(CKeyID(key.GetPubKey().GetID()));
}

BOOST_FIXTURE_TEST_SUITE(lottery_security_tests, TestingSetup)

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
    BOOST_CHECK(lottery::SignHeartbeat(key, t, script, "alice", 99, sig, true));
    BOOST_CHECK(lottery::VerifyHeartbeatSig(script, t, "alice", 99, sig, true));
    BOOST_CHECK(lottery::VerifyHeartbeatSig(script, t, "ALICE", 99, sig));
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(script, t, "alice", 99, sig, false));
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(script, t, "alice", 98, sig));
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(script, t + 1, "alice", 99, sig));
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(script, t, "bob", 99, sig));
    std::vector<unsigned char> empty;
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(script, t, "alice", 99, empty));
    std::vector<unsigned char> sigUnverified;
    BOOST_CHECK(lottery::SignHeartbeat(key, t, script, "alice", 99, sigUnverified, false));
    BOOST_CHECK(lottery::VerifyHeartbeatSig(script, t, "alice", 99, sigUnverified, false));
    BOOST_CHECK(!lottery::VerifyHeartbeatSig(script, t, "alice", 99, sigUnverified, true));

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

BOOST_AUTO_TEST_CASE(unverified_heartbeat_has_zero_chance)
{
    lottery::GetAllowlist().Reset();
    lottery::GetRegistry().Reset();
    CKey key;
    key.MakeNewKey(true);
    const CScript script = P2PKHFromKey(key);
    BOOST_CHECK(!lottery::GetRegistry().Heartbeat(script, 10, "ghost", 0, false));
    BOOST_CHECK_EQUAL(lottery::GetRegistry().Count(10), 0U);
    BOOST_CHECK(lottery::GetRegistry().Heartbeat(script, 10, "ghost", 0, true));
    BOOST_CHECK_EQUAL(lottery::GetRegistry().Count(10), 1U);
}

BOOST_AUTO_TEST_CASE(invite_list_cannot_exclude_verified_wallet)
{
    lottery::GetAllowlist().Reset();
    lottery::GetRegistry().Reset();
    CKey key;
    key.MakeNewKey(true);
    const CScript script = P2PKHFromKey(key);
    std::string err;
    BOOST_CHECK(lottery::GetAllowlist().Add("alice", 0, err, false));
    BOOST_CHECK(lottery::GetAllowlist().Allows("ghost", 0));
    BOOST_CHECK(lottery::GetRegistry().Heartbeat(script, 10, "ghost", 0, true));
    BOOST_CHECK_EQUAL(lottery::GetRegistry().Count(10), 1U);
}

BOOST_AUTO_TEST_CASE(slot_seconds_is_sixty_and_compile_time)
{
    BOOST_CHECK_EQUAL(lottery::SLOT_SECONDS, 60);
    BOOST_CHECK_EQUAL(lottery::MAX_FUTURE_SLOTS, 1);
}

BOOST_AUTO_TEST_CASE(block_time_must_sit_in_height_slot)
{
    const int64_t genesis = 1789197360; // frozen main genesis nTime
    const int height = 1;
    const int64_t slotStart = lottery::SlotStartTime(height, genesis);
    const int64_t now = slotStart + 5;
    CValidationState state;

    BOOST_CHECK(lottery::CheckBlockTime(height, slotStart, genesis, now, false, state));
    BOOST_CHECK(lottery::CheckBlockTime(height, slotStart + 59, genesis, now, false, state));

    CValidationState tooOld;
    BOOST_CHECK(!lottery::CheckBlockTime(height, slotStart - 1, genesis, now, false, tooOld));
    BOOST_CHECK_EQUAL(tooOld.GetRejectReason(), "bad-lottery-slot-time");

    CValidationState tooNewSlot;
    BOOST_CHECK(!lottery::CheckBlockTime(height, slotStart + 60, genesis, now, false, tooNewSlot));
    BOOST_CHECK_EQUAL(tooNewSlot.GetRejectReason(), "bad-lottery-slot-time");
}

BOOST_AUTO_TEST_CASE(clock_advance_cannot_mint_many_future_slots)
{
    const int64_t genesis = 1789197360;
    const int64_t now = lottery::SlotStartTime(1, genesis) + 10;
    CValidationState state;
    // Height 3 is two slots ahead of height 1 — rejected.
    const int64_t futureStart = lottery::SlotStartTime(3, genesis);
    BOOST_CHECK(!lottery::CheckBlockTime(3, futureStart, genesis, now, false, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "time-too-new");
}

BOOST_AUTO_TEST_CASE(regtest_skips_slot_lock)
{
    CValidationState state;
    BOOST_CHECK(lottery::CheckBlockTime(99, 1, 1789197360, 10, true, state));
}

BOOST_AUTO_TEST_CASE(historical_slot_is_a_function_of_height_not_peer_data)
{
    const int64_t genesis = 1789197360;
    BOOST_CHECK_EQUAL(lottery::SlotFromHeight(1, genesis), lottery::SlotFromTime(genesis) + 1);
    BOOST_CHECK_EQUAL(lottery::SlotStartTime(2, genesis), lottery::SlotStartTime(1, genesis) + lottery::SLOT_SECONDS);
    // Peer messages cannot change SLOT_SECONDS; it is a compile-time constant.
    BOOST_CHECK_EQUAL(lottery::SLOT_SECONDS, 60);
}

BOOST_AUTO_TEST_SUITE_END()

struct LotteryForkTestingSetup : public TestingSetup {
    LotteryForkTestingSetup() : TestingSetup(CBaseChainParams::REGTEST)
    {
        // DisconnectBlock always reads asset undo via passetsdb. Unit
        // TestingSetup never allocated it; a same-height reorg needs it.
        passetsdb = new CAssetsDB(1 << 20, true);
    }
    ~LotteryForkTestingSetup()
    {
        delete passetsdb;
        passetsdb = nullptr;
    }
};

BOOST_FIXTURE_TEST_SUITE(lottery_fork_tests, LotteryForkTestingSetup)

// A then B, same parent, equal work, hash(B) < hash(A) → tip becomes B.
// B is unrequested (AcceptBlock fHasMoreWork). CMPCTBLOCK uses PreferLotteryFork.
BOOST_AUTO_TEST_CASE(acceptblock_equal_work_smaller_hash_wins)
{
    const CChainParams& chainparams = GetParams();
    CKey key;
    key.MakeNewKey(true);
    const CScript script = P2PKHFromKey(key);
    lottery::GetRegistry().Reset();
    BOOST_REQUIRE(lottery::GetRegistry().Heartbeat(script, GetTime(), "alice", 0, true));

    std::unique_ptr<CBlockTemplate> tmpl;
    {
        LOCK(cs_main);
        tmpl = BlockAssembler(chainparams).CreateNewBlock(script);
    }
    BOOST_REQUIRE(tmpl);
    CBlock assembled = tmpl->block;
    unsigned int extraNonce = 0;
    {
        LOCK(cs_main);
        IncrementExtraNonce(&assembled, chainActive.Tip(), extraNonce);
    }

    CBlock blockA = assembled;
    CBlock blockB = assembled;
    blockB.nNonce = blockA.nNonce + 1;
    if (!(blockB.GetHash() < blockA.GetHash())) {
        std::swap(blockA, blockB);
    }
    BOOST_REQUIRE(blockB.GetHash() < blockA.GetHash());
    BOOST_REQUIRE(blockA.hashPrevBlock == blockB.hashPrevBlock);
    BOOST_REQUIRE(blockA.hashPrevBlock == chainActive.Tip()->GetBlockHash());

    const uint256 hashA = blockA.GetHash();
    const uint256 hashB = blockB.GetHash();

    bool fNew = false;
    BOOST_REQUIRE(ProcessNewBlock(chainparams, std::make_shared<const CBlock>(blockA), true, &fNew));
    {
        LOCK(cs_main);
        BOOST_REQUIRE(chainActive.Tip()->GetBlockHash() == hashA);
        BOOST_CHECK(GetBlockProof(*chainActive.Tip()) == arith_uint256(1));
    }

    // Unrequested: AcceptBlock used to drop equal-work (nChainWork > tip).
    fNew = false;
    BOOST_REQUIRE(ProcessNewBlock(chainparams, std::make_shared<const CBlock>(blockB), false, &fNew));
    BOOST_CHECK(fNew);
    {
        LOCK(cs_main);
        BOOST_CHECK(chainActive.Tip()->GetBlockHash() == hashB);
        BOOST_CHECK(PreferLotteryFork(chainActive.Tip(), mapBlockIndex[hashA]));
        BOOST_CHECK(chainActive.Tip()->nChainWork == mapBlockIndex[hashA]->nChainWork);
        BOOST_CHECK(GetBlockProof(*chainActive.Tip()) == arith_uint256(1));
    }
}

BOOST_AUTO_TEST_CASE(x_lookup_rejects_unverified_handle)
{
    xlookup::ClearLookupMocks();
    xlookup::SetLookupMock("ghost", xlookup::NOT_VERIFIED);
    xlookup::SetLookupMock("nftrvn", xlookup::VERIFIED);
    std::string err;
    BOOST_CHECK_EQUAL(xlookup::LookupHandle("ghost", err), xlookup::NOT_VERIFIED);
    BOOST_CHECK_EQUAL(xlookup::LookupHandle("NFTRVN", err), xlookup::VERIFIED);
    xlookup::ClearLookupMocks();
}

BOOST_AUTO_TEST_CASE(seed_stamp_binds_payout_and_handle)
{
    lottery::ResetAttestations();
    CKey key;
    key.MakeNewKey(true);
    lottery::SetAttestorKeyForTest(key);
    const CScript script = P2PKHFromKey(key);
    const uint160 id = lottery::IdFromScript(script);
    std::vector<unsigned char> sig;
    BOOST_CHECK(lottery::SignAttestation(id, "alice", sig));
    BOOST_CHECK(lottery::VerifyAttestationPub(key.GetPubKey(), id, "alice", sig));
    BOOST_CHECK(!lottery::VerifyAttestationPub(key.GetPubKey(), id, "bob", sig));
    CKey other;
    other.MakeNewKey(true);
    BOOST_CHECK(!lottery::VerifyAttestationPub(other.GetPubKey(), id, "alice", sig));
    lottery::ResetAttestations();
    CKey empty;
    lottery::SetAttestorKeyForTest(empty);
}

BOOST_AUTO_TEST_CASE(baked_seed_produce_does_not_need_session)
{
    BOOST_CHECK(!xsession::SessionIsXVerified());
    BOOST_CHECK(!lottery::GetRegistry().LocalEligible());
    BOOST_CHECK(!lottery::IsBakedSeed());
    BOOST_CHECK(!lottery::MayProduceBlock());

    lottery::SetBakedSeedForTest(true);
    BOOST_CHECK(lottery::IsBakedSeed());
    BOOST_CHECK(!xsession::SessionIsXVerified());
    BOOST_CHECK(!lottery::GetRegistry().LocalEligible());
    BOOST_CHECK(lottery::MayProduceBlock());
    BOOST_CHECK(!lottery::GetRegistry().HeartbeatLocal(10));
    lottery::SetBakedSeedForTest(false);
    BOOST_CHECK(!lottery::IsBakedSeed());
    BOOST_CHECK(!lottery::MayProduceBlock());
}

BOOST_AUTO_TEST_CASE(baked_seed_block_sig_binds_height_and_set)
{
    CKey key;
    key.MakeNewKey(true);
    lottery::SetAttestorKeyForTest(key);
    std::vector<uint160> ids;
    ids.push_back(lottery::IdFromScript(P2PKHFromKey(key)));
    const uint256 prev = uint256S("aa");
    const uint256 d = lottery::SeedBlockDigest(1, prev, ids);
    std::vector<unsigned char> sig;
    BOOST_CHECK(key.SignCompact(d, sig));
    BOOST_CHECK(lottery::VerifySeedBlockSig(key.GetPubKey(), 1, prev, ids, sig));
    BOOST_CHECK(!lottery::VerifySeedBlockSig(key.GetPubKey(), 2, prev, ids, sig));
    CKey other;
    other.MakeNewKey(true);
    BOOST_CHECK(!lottery::VerifySeedBlockSig(other.GetPubKey(), 1, prev, ids, sig));
    CKey empty;
    lottery::SetAttestorKeyForTest(empty);
}

BOOST_AUTO_TEST_SUITE_END()
