// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_XLOOKUP_H
#define XCOIN_XLOOKUP_H

#include <string>

/**
 * Seed-side lookup of a public X profile.
 *
 * Official wallets still Sign in with X locally. This layer asks X
 * "does this handle actually have a blue check?" so a custom binary
 * that flips verified=true cannot enter the hat with a made-up name.
 *
 * Only the seed (or any node with -xlookupbearer=) calls X.
 * Other nodes never need an API token.
 */
namespace xlookup {

enum Status {
    VERIFIED = 0,
    NOT_VERIFIED = 1,
    LOOKUP_FAILED = 2,
};

/** True if this process can call X (bearer or XCOIN_X_BEARER). */
bool LookupEnabled();

/**
 * Look up `handle` on X (or a test mock).
 * Caches VERIFIED for 1 hour and NOT_VERIFIED for 10 minutes.
 */
Status LookupHandle(const std::string& handle, std::string& err);

/** Unit tests: pin a handle without touching the network. */
void SetLookupMock(const std::string& handle, Status status);
void ClearLookupMocks();

} // namespace xlookup

#endif // XCOIN_XLOOKUP_H
