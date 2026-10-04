// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_raven.h>
#include <xprice.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(xprice_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(parse_live_neoxex_shape)
{
    // Shape verified against GET https://neoxa.exchange/api/exchange/ticker/XFER_USDT
    const char* json =
        "{\"success\":true,\"pair\":\"XFER_USDT\",\"ticker\":{"
        "\"lastPrice\":0.00005,\"change\":0.000032,\"changePercent\":177.7,"
        "\"high24h\":0.00005,\"low24h\":0.0000165,\"bestBid\":0.00003,\"bestAsk\":0.00005}}";
    double px = 0;
    std::string err;
    BOOST_CHECK(xprice::ParseLastPrice(json, px, err));
    BOOST_CHECK_CLOSE(px, 0.00005, 0.0001);
    BOOST_CHECK(err.empty());
    BOOST_CHECK_EQUAL(xprice::FormatPrice(px), "0.00005");
    BOOST_CHECK_EQUAL(xprice::FormatUsdt(7525804.51994829 * px), "376.29");
}

BOOST_AUTO_TEST_CASE(parse_rejects_bad_ticker)
{
    double px = 1;
    std::string err;
    BOOST_CHECK(!xprice::ParseLastPrice("", px, err));
    BOOST_CHECK(!xprice::ParseLastPrice("{\"success\":false,\"pair\":\"XFER_USDT\",\"ticker\":{\"lastPrice\":1}}", px, err));
    BOOST_CHECK(!xprice::ParseLastPrice("{\"success\":true,\"pair\":\"BTC_USDT\",\"ticker\":{\"lastPrice\":1}}", px, err));
    BOOST_CHECK(!xprice::ParseLastPrice("{\"success\":true,\"pair\":\"XFER_USDT\",\"ticker\":{\"lastPrice\":0}}", px, err));
    BOOST_CHECK(!xprice::ParseLastPrice("{\"success\":true,\"pair\":\"XFER_USDT\",\"ticker\":{}}", px, err));
    BOOST_CHECK(!xprice::ParseLastPrice("{\"success\":true,\"ticker\":{\"lastPrice\":\"nope\"}}", px, err));
    BOOST_CHECK_EQUAL(px, 1);
}

BOOST_AUTO_TEST_CASE(parse_string_last_price)
{
    double px = 0;
    std::string err;
    BOOST_CHECK(xprice::ParseLastPrice(
        "{\"success\":true,\"pair\":\"XFER_USDT\",\"ticker\":{\"lastPrice\":\"0.01\"}}", px, err));
    BOOST_CHECK_CLOSE(px, 0.01, 0.0001);
    BOOST_CHECK_EQUAL(xprice::FormatPrice(px), "0.01");
    BOOST_CHECK_EQUAL(xprice::FormatUsdt(0.004), "0.004000");
}

BOOST_AUTO_TEST_SUITE_END()
