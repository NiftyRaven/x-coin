// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_XPRICE_H
#define RAVEN_XPRICE_H

#include <string>

/** Public XFER/USDT display price. No key, no authenticated call. */
namespace xprice {

/** GET this URL. Documented at https://neoxa.exchange/api-docs */
const char* TickerUrl();

/**
 * Read ticker.lastPrice from a NeoxEX ticker body.
 * Requires success and pair XFER_USDT when those fields are present.
 * Rejects missing, zero, negative, and non-finite prices.
 */
bool ParseLastPrice(const std::string& json, double& lastPriceOut, std::string& err);

/** Display string for the XFER/USDT price. */
std::string FormatPrice(double price);

/** Display string for a USDT amount converted from XFER. */
std::string FormatUsdt(double amount);

} // namespace xprice

#endif
