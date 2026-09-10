// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <assets/assetdb.h>
#include <assets/assets.h>
#include <assets/xaccount.h>
#include <key.h>
#include <lottery.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <serialize.h>
#include <streams.h>
#include <test/test_raven.h>
#include <validation.h>

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

BOOST_AUTO_TEST_CASE(dummy_prevout_unique_per_handle)
{
    const COutPoint a = MakeXAccountDummyPrevout("nftrvn");
    const COutPoint b = MakeXAccountDummyPrevout("alice");
    const COutPoint a2 = MakeXAccountDummyPrevout("nftrvn");
    BOOST_CHECK(a == a2);
    BOOST_CHECK(a != b);
    BOOST_CHECK(IsXAccountDummyPrevout(a));
    BOOST_CHECK(IsXAccountDummyPrevout(b));
    BOOST_CHECK(!a.IsNull());
    BOOST_CHECK(!b.hash.IsNull());
    BOOST_CHECK(a.n == XACCOUNT_DUMMY_N);
    COutPoint coinbase;
    BOOST_CHECK(coinbase.IsNull());
    BOOST_CHECK(!IsXAccountDummyPrevout(coinbase));
}

BOOST_AUTO_TEST_CASE(dummy_vin_roundtrip_is_not_bip144_marker)
{
    CMutableTransaction mtx;
    mtx.nVersion = CTransaction::CURRENT_VERSION;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = MakeXAccountDummyPrevout("nftrvn");
    mtx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 0;
    mtx.vout[0].scriptPubKey = MakeXAccountAssignmentScript("nftrvn");
    const CTransaction tx(mtx);
    BOOST_CHECK(!tx.IsCoinBase());
    BOOST_CHECK(!tx.vin.empty());
    BOOST_CHECK(!tx.HasWitness());

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    CTransaction tx2(deserialize, ss);
    BOOST_CHECK_EQUAL(tx2.vin.size(), 1u);
    BOOST_CHECK(tx2.vin[0].prevout == tx.vin[0].prevout);
    BOOST_CHECK(!tx2.IsCoinBase());
    BOOST_CHECK_EQUAL(tx2.vout.size(), 1u);

    std::string err;
    BOOST_CHECK(!CheckXAccountDummyInputs(tx, err));
    BOOST_CHECK(err.find("xid-dummy") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(identity_claim_full_tx_roundtrip)
{
    CKey key;
    key.MakeNewKey(true);
    const CScript dest = GetScriptForDestination(key.GetPubKey().GetID());
    CNewAsset asset("NFTRVN", MAIN_ASSET_CIRCULATING_AMOUNT, 0, 0, 0, "");

    CMutableTransaction mtx;
    mtx.nVersion = CTransaction::CURRENT_VERSION;
    mtx.nLockTime = 0;
    CTxIn dummy;
    dummy.prevout = MakeXAccountDummyPrevout("nftrvn");
    dummy.nSequence = CTxIn::SEQUENCE_FINAL;
    mtx.vin.push_back(dummy);
    mtx.vout.push_back(CTxOut(0, MakeXAccountAssignmentScript("nftrvn")));
    CScript owner = dest;
    asset.ConstructOwnerTransaction(owner);
    mtx.vout.push_back(CTxOut(0, owner));
    CScript issue = dest;
    asset.ConstructTransaction(issue);
    mtx.vout.push_back(CTxOut(0, issue));

    const CTransaction tx(mtx);
    BOOST_CHECK(!tx.IsCoinBase());
    BOOST_CHECK(!tx.HasWitness());
    BOOST_CHECK_EQUAL(tx.vin.size(), 1u);
    BOOST_CHECK(tx.IsNewAsset());
    std::string handle;
    BOOST_CHECK(IsXAccountIdentityClaim(tx, &handle));
    BOOST_CHECK_EQUAL(handle, "nftrvn");
    BOOST_CHECK(IsXAccountDummyInput(tx, 0));
    std::string err;
    BOOST_CHECK(CheckXAccountDummyInputs(tx, err));
    BOOST_CHECK(err.empty());
    for (const auto& out : tx.vout)
        BOOST_CHECK_EQUAL(out.nValue, 0);

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    CTransaction tx2(deserialize, ss);
    BOOST_CHECK_EQUAL(tx2.vin.size(), 1u);
    BOOST_CHECK(tx2.vin[0].prevout == tx.vin[0].prevout);
    BOOST_CHECK(IsXAccountIdentityClaim(tx2));
    BOOST_CHECK(tx2.IsNewAsset());
}

BOOST_AUTO_TEST_CASE(assignment_reloads_from_assets_db)
{
    CAssetsDB* prev = passetsdb;
    passetsdb = new CAssetsDB(1 << 20, true);
    BOOST_CHECK(AddXAccountAssignment("alice", "ALICE"));
    std::string name;
    BOOST_CHECK(CheckIfXAccountAssigned("alice", &name));
    BOOST_CHECK_EQUAL(name, "ALICE");

    // Drop the in-memory row (and the DB row), then put only the DB row back.
    // CheckIfXAccountAssigned must reload ALICE without a new session-main map.
    BOOST_CHECK(RemoveXAccountAssignment("alice"));
    BOOST_CHECK(!CheckIfXAccountAssigned("alice", &name));
    BOOST_CHECK(passetsdb->WriteXAccountAssignment("alice", "ALICE"));
    name.clear();
    BOOST_CHECK(CheckIfXAccountAssigned("alice", &name));
    BOOST_CHECK_EQUAL(name, "ALICE");

    BOOST_CHECK(RemoveXAccountAssignment("alice"));
    delete passetsdb;
    passetsdb = prev;
}

BOOST_AUTO_TEST_SUITE_END()
