// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_XOAUTH_TOKEN_H
#define RAVEN_XOAUTH_TOKEN_H

#include <string>

/**
 * Access token held across the async GET /2/users/me call.
 * onTokenResponse copies the token into the Authorization header and
 * keeps that same string until users/me returns. Clearing the hold when
 * the request starts is the 1.0.24 bug: SetClaimBearer then stored empty.
 */
struct XOAuthUsersMeHandoff {
    std::string held;
    std::string sentToUsersMe;

    void onTokenResponse(const std::string& token)
    {
        held = token;
        sentToUsersMe = held;
    }

    void clear()
    {
        held.clear();
        sentToUsersMe.clear();
    }

    bool holdSurvivesUsersMe() const
    {
        return !held.empty() && held == sentToUsersMe;
    }
};

inline bool XOAuthBearerAfterUsersMe(const XOAuthUsersMeHandoff& handoff, std::string& bearerOut, std::string& err)
{
    if (!handoff.holdSurvivesUsersMe()) {
        err = "Sign in with X finished, but the access token was empty. Claim cannot continue. Sign in again.";
        bearerOut.clear();
        return false;
    }
    bearerOut = handoff.held;
    return true;
}

#endif
