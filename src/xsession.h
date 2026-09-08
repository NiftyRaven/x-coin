// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_XSESSION_H
#define XCOIN_XSESSION_H

#include <string>

class UniValue;

/**
 * Local proof that this node signed in with X.
 *
 * Produced only by the OAuth PKCE flow (or a regtest mock of GET /2/users/me).
 * HMAC-SHA256 over user id + username + expiry with a datadir secret
 * (xsession.key). Typed handles are not a session.
 */
namespace xsession {

struct Session {
    std::string userId;
    std::string username; // normalized lowercase, no '@'
    int64_t expiresAt = 0;
    std::string proofHex;
};

bool IsRegtest();

std::string SecretPath();
std::string SessionPath();

bool EnsureSecret(std::string& err);
std::string ComputeProof(const std::string& userId, const std::string& username, int64_t expiresAt);

/** Persist a session from a real or mock users/me payload. */
bool SaveSession(const std::string& userId, const std::string& username, int64_t expiresAt, std::string& err);
bool LoadSession(Session& out, std::string& err);
bool HasValidSession();
void ClearSession();

/** Parse X API GET /2/users/me JSON: {"data":{"id":"...","username":"..."}} */
bool ParseUsersMe(const std::string& json, std::string& userId, std::string& username, std::string& err);

/**
 * Inject a mock users/me. Allowed only on regtest.
 * `payload` is either JSON as above, or "handle" / "handle:userid".
 */
bool ApplyUsersMePayload(const std::string& payload, std::string& err, UniValue* parsedOut = nullptr);

/** -xoauthmock= on regtest at startup. */
void ApplyStartupArgs();

/** Set lottery local identity from the signed-in session (not from typed -xaccount). */
void BindLotteryFromSession();

/**
 * True if this node has a valid Sign in with X session.
 * Required to send, receive, and prove asset ownership.
 * Lottery eligibility is session plus the X-Verified allowlist.
 */
bool RequireSession(std::string& err);

/**
 * True if `handle` is the signed-in username.
 * Fails closed: no session, expired session, or mismatch → false.
 */
bool RequireHandle(const std::string& handle, std::string& err);

/** Normalized session username, or empty. */
std::string SignedInHandle();
std::string SignedInUserId();

} // namespace xsession

#endif
