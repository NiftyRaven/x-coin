// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xprice.h"

#include <univalue.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace xprice {

const char* TickerUrl()
{
    return "https://neoxa.exchange/api/exchange/ticker/XFER_USDT";
}

static bool ReadPositive(const UniValue& v, double& out)
{
    if (v.isNum()) {
        out = v.get_real();
    } else if (v.isStr()) {
        const std::string s = v.get_str();
        if (s.empty())
            return false;
        char* end = nullptr;
        out = std::strtod(s.c_str(), &end);
        if (!end || end == s.c_str() || *end != '\0')
            return false;
    } else {
        return false;
    }
    if (!std::isfinite(out) || !(out > 0.0) || out > 1.0e9)
        return false;
    return true;
}

bool ParseLastPrice(const std::string& json, double& lastPriceOut, std::string& err)
{
    err.clear();
    UniValue u;
    if (!u.read(json) || !u.isObject()) {
        err = "ticker is not a JSON object";
        return false;
    }
    if (u.exists("success")) {
        const UniValue& ok = u["success"];
        const bool good = ok.isTrue() || (ok.isBool() && ok.get_bool()) || (ok.isNum() && ok.get_real() != 0.0);
        if (!good) {
            err = "ticker reported failure";
            return false;
        }
    }
    if (u.exists("pair")) {
        const std::string pair = u["pair"].getValStr();
        if (pair != "XFER_USDT") {
            err = "unexpected ticker pair";
            return false;
        }
    }
    if (!u.exists("ticker") || !u["ticker"].isObject()) {
        err = "ticker object missing";
        return false;
    }
    const UniValue& ticker = u["ticker"];
    double price = 0.0;
    if (!ticker.exists("lastPrice") || !ReadPositive(ticker["lastPrice"], price)) {
        err = "lastPrice missing";
        return false;
    }
    lastPriceOut = price;
    return true;
}

static std::string TrimTrailingZeros(std::string s)
{
    const size_t dot = s.find('.');
    if (dot == std::string::npos)
        return s;
    while (!s.empty() && s.back() == '0')
        s.pop_back();
    if (!s.empty() && s.back() == '.')
        s.push_back('0');
    return s;
}

std::string FormatPrice(double price)
{
    if (!std::isfinite(price) || !(price > 0.0))
        return std::string();
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.8f", price);
    return TrimTrailingZeros(buf);
}

std::string FormatUsdt(double amount)
{
    if (!std::isfinite(amount) || amount < 0.0)
        amount = 0.0;
    char buf[64];
    if (amount >= 0.01 || amount == 0.0)
        std::snprintf(buf, sizeof(buf), "%.2f", amount);
    else
        std::snprintf(buf, sizeof(buf), "%.6f", amount);
    return buf;
}

} // namespace xprice
