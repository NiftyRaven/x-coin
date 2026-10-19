// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <assets/assets.h>
#include <assets/xaccount.h>
#include <key.h>
#include <lottery.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <serialize.h>
#include <streams.h>
#include <test/test_raven.h>
#include <univalue.h>
#include <util.h>
#include <utilstrencodings.h>

#include <boost/test/unit_test.hpp>

#include <fstream>
#include <sstream>

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

static CTransaction BuildClaimTx(const std::string& handle, const CKeyID& dest, const std::string& root,
                                 const CKeyID* ownerDest, const std::vector<unsigned char>* proof)
{
    CNewAsset asset(root, MAIN_ASSET_CIRCULATING_AMOUNT, 0, 0, 0, "");
    CMutableTransaction mtx;
    mtx.nVersion = CTransaction::CURRENT_VERSION;
    mtx.nLockTime = 0;
    CTxIn dummy;
    dummy.prevout = MakeXAccountDummyPrevout(handle);
    dummy.nSequence = CTxIn::SEQUENCE_FINAL;
    mtx.vin.push_back(dummy);
    if (proof)
        mtx.vout.push_back(CTxOut(0, MakeXAccountAssignmentScriptWithProof(handle, *proof)));
    else
        mtx.vout.push_back(CTxOut(0, MakeXAccountAssignmentScript(handle)));
    const CKeyID& ownerId = ownerDest ? *ownerDest : dest;
    CScript ownerScript = GetScriptForDestination(ownerId);
    asset.ConstructOwnerTransaction(ownerScript);
    mtx.vout.push_back(CTxOut(0, ownerScript));
    CScript issue = GetScriptForDestination(dest);
    asset.ConstructTransaction(issue);
    mtx.vout.push_back(CTxOut(0, issue));
    return CTransaction(mtx);
}

static std::vector<unsigned char> SignedProof(const CKey& key, uint8_t keyId, const std::string& handle, uint64_t userId,
                                              const uint160& addr, const std::string& root, uint32_t expiry)
{
    std::vector<unsigned char> sig;
    const uint256 digest = XClaimProofDigest(handle, userId, addr, root, expiry, keyId);
    BOOST_CHECK(key.SignCompact(digest, sig));
    std::vector<unsigned char> payload;
    BOOST_CHECK(BuildXClaimProofPayload(keyId, userId, expiry, sig, payload));
    return payload;
}

static std::vector<unsigned char> FirstPush(const CScript& script)
{
    CScript::const_iterator pc = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> item;
    BOOST_CHECK(script.GetOp(pc, opcode, item));
    BOOST_CHECK(script.GetOp(pc, opcode, item));
    return item;
}

BOOST_AUTO_TEST_CASE(xclaim_proof_digest_vector)
{
    fs::path jsonPath = fs::path(__FILE__).parent_path().parent_path() / "data" / "xpf1-vector.json";
    if (!fs::exists(jsonPath))
        jsonPath = fs::path("src/test/data/xpf1-vector.json");
    if (!fs::exists(jsonPath))
        jsonPath = fs::path("test/data/xpf1-vector.json");
    std::ifstream in(jsonPath.string());
    BOOST_REQUIRE(in.good());
    std::stringstream buf;
    buf << in.rdbuf();
    UniValue v;
    BOOST_REQUIRE(v.read(buf.str()));
    const std::string handle = v["handle"].get_str();
    const uint64_t userId = v["userId"].get_int64();
    const std::vector<unsigned char> raw = ParseHex(v["hash160"].get_str());
    const uint160 addr(raw);
    const std::string root = v["root"].get_str();
    const uint32_t expiry = static_cast<uint32_t>(v["expiry"].get_int64());
    const uint8_t keyId = static_cast<uint8_t>(v["keyId"].get_int());
    const uint256 digest = XClaimProofDigest(handle, userId, addr, root, expiry, keyId);
    BOOST_CHECK_EQUAL(HexStr(digest.begin(), digest.end()), v["digest"].get_str());
    BOOST_CHECK_EQUAL(v["tag"].get_str(), "xcoin-xpf-v1");
    BOOST_CHECK_EQUAL(v["version"].get_int(), 1);
}

BOOST_AUTO_TEST_CASE(xclaim_proof_policy)
{
    CKey key;
    key.MakeNewKey(true);
    const CPubKey pub = key.GetPubKey();
    const CKeyID id = pub.GetID();
    CKey other;
    other.MakeNewKey(true);
    const CKeyID otherId = other.GetPubKey().GetID();

    const std::string handle = "nftrvn";
    const std::string root = "NFTRVN";
    const uint64_t userId = 42;
    const int64_t now = 1700000000;
    const uint32_t expiry = static_cast<uint32_t>(now + 900);

    const uint8_t keyId = 1;
    const std::vector<unsigned char> payload = SignedProof(key, keyId, handle, userId, id, root, expiry);
    const CTransaction plain = BuildClaimTx(handle, id, root, nullptr, nullptr);
    const CTransaction proved = BuildClaimTx(handle, id, root, nullptr, &payload);

    std::string hPlain, hProved, xidPlain, xidProved;
    BOOST_CHECK(IsXAccountIdentityClaim(plain, &hPlain));
    BOOST_CHECK(IsXAccountIdentityClaim(proved, &hProved));
    BOOST_CHECK_EQUAL(hPlain, hProved);
    BOOST_CHECK_EQUAL(hPlain, handle);
    BOOST_CHECK(ParseXAccountAssignment(plain, xidPlain));
    BOOST_CHECK(ParseXAccountAssignment(proved, xidProved));
    BOOST_CHECK_EQUAL(xidPlain, xidProved);
    BOOST_CHECK_EQUAL(xidPlain, handle);
    BOOST_CHECK(FirstPush(plain.vout[0].scriptPubKey) == FirstPush(proved.vout[0].scriptPubKey));

    XClaimProof parsed;
    BOOST_CHECK(!ParseXClaimProof(plain, parsed));
    BOOST_CHECK(ParseXClaimProof(proved, parsed));
    BOOST_CHECK_EQUAL(parsed.keyId, keyId);
    BOOST_CHECK_EQUAL(parsed.userId, userId);
    BOOST_CHECK_EQUAL(parsed.expiry, expiry);
    BOOST_CHECK_EQUAL(parsed.sig.size(), 65u);

    std::map<uint8_t, CPubKey> pubs;
    pubs[keyId] = pub;
    SetXClaimProofPubsForTest(pubs);

    std::string err;
    BOOST_CHECK(CheckXClaimProof(proved, now, err));
    BOOST_CHECK(err.empty());

    BOOST_CHECK(!CheckXClaimProof(plain, now, err));
    BOOST_CHECK_EQUAL(err, "xclaim-proof-missing");

    pubs[keyId] = other.GetPubKey();
    SetXClaimProofPubsForTest(pubs);
    BOOST_CHECK(!CheckXClaimProof(proved, now, err));
    BOOST_CHECK_EQUAL(err, "xclaim-proof-bad-sig");
    pubs.clear();
    pubs[2] = pub;
    SetXClaimProofPubsForTest(pubs);
    BOOST_CHECK(!CheckXClaimProof(proved, now, err));
    BOOST_CHECK_EQUAL(err, "xclaim-proof-bad-sig");
    pubs[keyId] = pub;
    SetXClaimProofPubsForTest(pubs);

    const uint8_t keyId2 = 2;
    const std::vector<unsigned char> secondPayload = SignedProof(other, keyId2, handle, userId, id, root, expiry);
    const CTransaction second = BuildClaimTx(handle, id, root, nullptr, &secondPayload);
    pubs[keyId2] = other.GetPubKey();
    SetXClaimProofPubsForTest(pubs);
    BOOST_CHECK(CheckXClaimProof(second, now, err));
    BOOST_CHECK(CheckXClaimProof(proved, now, err));

    const std::vector<unsigned char> expiredPayload = SignedProof(key, keyId, handle, userId, id, root, static_cast<uint32_t>(now));
    const CTransaction expired = BuildClaimTx(handle, id, root, nullptr, &expiredPayload);
    BOOST_CHECK(!CheckXClaimProof(expired, now, err));
    BOOST_CHECK_EQUAL(err, "xclaim-proof-expired");

    const std::vector<unsigned char> pastPayload = SignedProof(key, keyId, handle, userId, id, root, static_cast<uint32_t>(now - 1));
    const CTransaction past = BuildClaimTx(handle, id, root, nullptr, &pastPayload);
    BOOST_CHECK(!CheckXClaimProof(past, now, err));
    BOOST_CHECK_EQUAL(err, "xclaim-proof-expired");

    const std::vector<unsigned char> farPayload = SignedProof(key, keyId, handle, userId, id, root, static_cast<uint32_t>(now + XCLAIM_PROOF_MAX_FUTURE + 1));
    const CTransaction far = BuildClaimTx(handle, id, root, nullptr, &farPayload);
    BOOST_CHECK(!CheckXClaimProof(far, now, err));
    BOOST_CHECK_EQUAL(err, "xclaim-proof-expired");

    const std::vector<unsigned char> edgePayload = SignedProof(key, keyId, handle, userId, id, root, static_cast<uint32_t>(now + XCLAIM_PROOF_MAX_FUTURE));
    const CTransaction edge = BuildClaimTx(handle, id, root, nullptr, &edgePayload);
    BOOST_CHECK(CheckXClaimProof(edge, now, err));

    const std::vector<unsigned char> addrPayload = SignedProof(key, keyId, handle, userId, otherId, root, expiry);
    const CTransaction wrongAddr = BuildClaimTx(handle, id, root, nullptr, &addrPayload);
    BOOST_CHECK(!CheckXClaimProof(wrongAddr, now, err));
    BOOST_CHECK_EQUAL(err, "xclaim-proof-bad-sig");

    const std::vector<unsigned char> handlePayload = SignedProof(key, keyId, "alice", userId, id, root, expiry);
    const CTransaction wrongHandle = BuildClaimTx(handle, id, root, nullptr, &handlePayload);
    BOOST_CHECK(!CheckXClaimProof(wrongHandle, now, err));
    BOOST_CHECK_EQUAL(err, "xclaim-proof-bad-sig");

    const std::vector<unsigned char> rootPayload = SignedProof(key, keyId, handle, userId, id, "OTHER", expiry);
    const CTransaction wrongRoot = BuildClaimTx(handle, id, root, nullptr, &rootPayload);
    BOOST_CHECK(!CheckXClaimProof(wrongRoot, now, err));
    BOOST_CHECK_EQUAL(err, "xclaim-proof-bad-sig");

    const CTransaction split = BuildClaimTx(handle, id, root, &otherId, &payload);
    BOOST_CHECK(IsXAccountIdentityClaim(split));
    BOOST_CHECK(!CheckXClaimProof(split, now, err));
    BOOST_CHECK_EQUAL(err, "xclaim-proof-mismatch");

    const std::map<uint8_t, CPubKey> saved = XClaimProofPubs();
    SetXClaimProofPubsForTest({});
    BOOST_CHECK(!XClaimProofEnforced());
    BOOST_CHECK(!XClaimProofBlocksInclusion(plain));
    BOOST_CHECK(!XClaimProofBlocksInclusion(proved));
    SetXClaimProofPubsForTest(pubs);
    BOOST_CHECK(XClaimProofEnforced());
    // BlocksInclusion uses GetTime(), not the fixed `now` above.
    const uint32_t liveExpiry = static_cast<uint32_t>(GetTime() + 900);
    const std::vector<unsigned char> livePayload = SignedProof(key, keyId, handle, userId, id, root, liveExpiry);
    const CTransaction live = BuildClaimTx(handle, id, root, nullptr, &livePayload);
    BOOST_CHECK(XClaimProofBlocksInclusion(plain));
    BOOST_CHECK(!XClaimProofBlocksInclusion(live));
    SetXClaimProofPubsForTest(saved);

    const unsigned savedCarrier = nMaxDatacarrierBytes;
    const bool savedAccept = fAcceptDatacarrier;
    nMaxDatacarrierBytes = MAX_OP_RETURN_RELAY;
    fAcceptDatacarrier = true;
    std::string reason;
    BOOST_CHECK(proved.vout[0].scriptPubKey.size() > MAX_OP_RETURN_RELAY);
    BOOST_CHECK(proved.vout[0].scriptPubKey.size() <= MAX_XCLAIM_DATACARRIER_BYTES);
    BOOST_CHECK(IsStandardTx(proved, reason, true));
    BOOST_CHECK(IsStandardTx(plain, reason, true));

    CMutableTransaction junk(plain);
    std::vector<unsigned char> blob(90, 0x11);
    junk.vout[0].scriptPubKey = CScript() << OP_RETURN << blob;
    BOOST_CHECK(!IsStandardTx(CTransaction(junk), reason, true));
    BOOST_CHECK_EQUAL(reason, "scriptpubkey");

    CMutableTransaction extra(proved);
    extra.vout[0].scriptPubKey << std::vector<unsigned char>(1, 0x01);
    BOOST_CHECK(extra.vout[0].scriptPubKey.size() > MAX_OP_RETURN_RELAY);
    BOOST_CHECK(!IsStandardTx(CTransaction(extra), reason, true));
    BOOST_CHECK_EQUAL(reason, "scriptpubkey");
    txnouttype bareType;
    BOOST_CHECK(!::IsStandard(proved.vout[0].scriptPubKey, bareType, true));
    BOOST_CHECK(bareType == TX_NULL_DATA);
    BOOST_CHECK(IsStandardTx(proved, reason, true));
    nMaxDatacarrierBytes = savedCarrier;
    fAcceptDatacarrier = savedAccept;

    const std::string hint = XClaimSignerUnreachableMessage();
    BOOST_CHECK(hint.find("paused") != std::string::npos);
    BOOST_CHECK(hint.find("Launch") != std::string::npos);
    BOOST_CHECK(hint.find("existing names") != std::string::npos);
    BOOST_CHECK(hint.find("earn") == std::string::npos);
    BOOST_CHECK(hint.find("yield") == std::string::npos);
    BOOST_CHECK(hint.find("APY") == std::string::npos);
    BOOST_CHECK(hint.find("profit") == std::string::npos);
    BOOST_CHECK(hint.find("guaranteed") == std::string::npos);
    const std::string shown = FormatXClaimWalletError("Error: The transaction was rejected! Reason given: xclaim-proof-missing");
    BOOST_CHECK(shown.find("paused") != std::string::npos);
    BOOST_CHECK(shown.find("xclaim-proof-missing") != std::string::npos);
    BOOST_CHECK_EQUAL(FormatXClaimWalletError("wallet locked"), "wallet locked");

    std::string argErr;
    BOOST_CHECK(SetXClaimProofPubkeys({}, argErr));
    BOOST_CHECK(!XClaimProofEnforced());
    BOOST_CHECK(!SetXClaimProofPubkeys({"zz"}, argErr));
    BOOST_CHECK(argErr.find("id") != std::string::npos);
    const CPubKey otherPub = other.GetPubKey();
    const std::string pubHex = HexStr(pub.begin(), pub.end());
    const std::string otherHex = HexStr(otherPub.begin(), otherPub.end());
    const std::string both = "1:" + pubHex + ",2:" + otherHex;
    argErr.clear();
    BOOST_CHECK(SetXClaimProofPubkeys(std::vector<std::string>{both}, argErr));
    BOOST_CHECK(XClaimProofEnforced());
    BOOST_CHECK(XClaimProofPubs().size() == 2u);
    BOOST_CHECK(XClaimProofPubs().at(1) == pub);
    BOOST_CHECK(XClaimProofPubs().at(2) == otherPub);
    const std::vector<std::string> repeated{std::string("1:") + pubHex, std::string("2:") + otherHex};
    BOOST_CHECK(SetXClaimProofPubkeys(repeated, argErr));
    BOOST_CHECK(XClaimProofPubs().size() == 2u);
    BOOST_CHECK(!SetXClaimProofPubkeys(std::vector<std::string>{std::string("1:") + pubHex + ",1:" + otherHex}, argErr));
    BOOST_CHECK(argErr.find("duplicated") != std::string::npos);

    const std::string signerJson =
        std::string("{\"ok\":true,\"keyId\":1,\"userId\":42,\"expiry\":") + std::to_string(now + 900) +
        ",\"sig\":\"" + HexStr(parsed.sig.begin(), parsed.sig.end()) +
        "\",\"handle\":\"nftrvn\",\"root\":\"NFTRVN\",\"address\":\"yTestAddress\"}";
    std::vector<unsigned char> fromJson;
    BOOST_CHECK(XClaimProofFromSignerJson(signerJson, "nftrvn", 42, "yTestAddress", "NFTRVN", now, fromJson, argErr));
    BOOST_CHECK(fromJson.size() == XCLAIM_PROOF_PAYLOAD_SIZE);
    BOOST_CHECK(!XClaimProofFromSignerJson("{\"ok\":false,\"error\":\"sign-in-again\"}", "nftrvn", 42, "yTestAddress", "NFTRVN", now, fromJson, argErr));
    BOOST_CHECK(argErr.find("Sign in with X again") != std::string::npos);

    gArgs.ForceSetArg("-xclaimproofurls", "");
    BOOST_CHECK(XClaimProofUrls().empty());
    BOOST_CHECK(!XClaimProofShouldAttach() || XClaimProofEnforced());
    gArgs.ForceSetArg("-xclaimproofurls", std::string("https://xferchain.net/api/x/claim-proof,http://127.0.0.1:9/claim-proof"));
    BOOST_CHECK_EQUAL(XClaimProofUrls().size(), 2u);
    gArgs.ForceSetArg("-xclaimproofurls", "");

    SetXClaimProofPubsForTest(saved);
}

BOOST_AUTO_TEST_SUITE_END()
