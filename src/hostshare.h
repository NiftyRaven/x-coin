// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_HOSTSHARE_H
#define XCOIN_HOSTSHARE_H

#include "amount.h"
#include "uint256.h"

#include <string>
#include <vector>

/**
 * Wallet-only host/guest lottery share.
 * Consensus (hat, coinbase, XHB1/XVA1/XSD1) is unchanged.
 * A host opts in, sets a percent of their own mature lottery output,
 * and invites guests by X handle. Guests never enter the hat.
 */
namespace hostshare {

static const int MIN_PERCENT = 1;
static const int MAX_PERCENT = 100;
static const int DEFAULT_PERCENT = 20;
static const size_t MAX_GUESTS = 16;

struct GuestInfo {
    std::string handle;
    std::string asset;
    std::string address;
    bool ready;
    GuestInfo() : ready(false) {}
};

void Load();
void ResetForTest();

bool Enabled();
int Percent();
bool AssetIndexReady();

bool SetEnabled(bool on, std::string& err);
bool SetPercent(int percent, std::string& err);
bool InviteGuest(const std::string& handle, std::string& err);
bool RemoveGuest(const std::string& handle, std::string& err);
std::vector<GuestInfo> ListGuests();
std::vector<std::string> GuestHandles();

CAmount GuestPot(CAmount nValue, int percent);
CAmount GuestShare(CAmount pot, size_t nReady);

bool ResolveHolder(const std::string& handle, std::string& assetOut,
                   std::string& addressOut, std::string& err);

/** Write assetindex=1 and a request-reindex marker. needsRestart is true if the node must relaunch. */
bool EnableAssetIndex(std::string& err, bool& needsRestart);

#ifdef ENABLE_WALLET
/** Scan this wallet for mature lottery coinbases and send the guest pot. */
void MaybeShareMatureWins();
#endif

} // namespace hostshare

#endif // XCOIN_HOSTSHARE_H
