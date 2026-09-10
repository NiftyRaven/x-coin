// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_XSESSION_H
#define XCOIN_XSESSION_H

#include <cstdint>
#include <string>

class UniValue;

/**
 * Local proof that this node signed in with X.
 *
 * Produced only by the OAuth PKCE flow (or a regtest mock of GET /2/users/me).
 * HMAC-SHA256 over user id + username + expiry + X Verified fields with a
 * datadir secret (xsession.key). Typed handles are not a session.
 *
 * X Verified means X itself marked the account verified (blue check / X
 * Premium, plus business and government org checks). It is not the operator
 * invite list (addxverified). Official definition:
 * https://help.x.com/en/managing-your-account/about-x-bluecheck
 * https://help.x.com/en/rules-and-policies/verification-policy
 */
namespace xsession {

struct Session {
    std::string userId;
    std::string username; // normalized lowercase, no '@'
    int64_t expiresAt = 0;
    std::string proofHex;
    bool verified = false;          // users/me "verified"
    std::string verifiedType;       // users/me "verified_type": blue|business|government|…
    bool IsXVerified() const;
};

struct UsersMe {
    std::string userId;
    std::string username;
    bool verified = false;
    std::string verifiedType;
};

bool IsRegtest();

std::string SecretPath();
std::string SessionPath();

bool EnsureSecret(std::string& err);
std::string ComputeProof(const std::string& userId, const std::string& username, int64_t expiresAt,
                         bool verified, const std::string& verifiedType);

/** Persist a session from a real or mock users/me payload, including X Verified. */
bool SaveSession(const std::string& userId, const std::string& username, int64_t expiresAt,
                 std::string& err, bool verified = false, const std::string& verifiedType = "");
bool LoadSession(Session& out, std::string& err);
bool HasValidSession();
void ClearSession();

/**
 * Parse X API GET /2/users/me JSON.
 * Request user.fields=verified,verified_type. Example:
 * {"data":{"id":"...","username":"...","verified":true,"verified_type":"blue"}}
 */
bool ParseUsersMe(const std::string& json, UsersMe& out, std::string& err);

/** True for X's official checkmark types (blue / business / government) or verified==true. */
bool IsOfficialXVerified(bool verified, const std::string& verifiedType);

/**
 * Inject a mock users/me. Allowed only on regtest.
 * `payload` is JSON as above, or "handle" / "handle:userid" /
 * "handle:verified" / "handle:userid:verified" / "handle:unverified".
 * Compact NFTRVN with no flag defaults to verified=true (regtest).
 */
bool ApplyUsersMePayload(const std::string& payload, std::string& err, UniValue* parsedOut = nullptr);

/** -xoauthmock= on regtest at startup. */
void ApplyStartupArgs();

/** Set lottery local identity from the signed-in session (not from typed -xaccount). */
void BindLotteryFromSession();

/**
 * True if this node has a valid Sign in with X session.
 * Required to send, receive, and prove asset ownership.
 * Lottery eligibility is session plus X Verified (users/me.verified)
 * plus a running wallet. Unverified = zero chance. The operator invite
 * list cannot exclude a verified wallet.
 */
bool RequireSession(std::string& err);

/**
 * True if the signed-in session is X Verified (blue / business / government).
 * Does not consult the operator invite list.
 */
bool SessionIsXVerified();
bool RequireXVerified(std::string& err);

/**
 * True if `handle` is the signed-in username.
 * Fails closed: no session, expired session, or mismatch → false.
 */
bool RequireHandle(const std::string& handle, std::string& err);

/** Normalized session username, or empty. */
std::string SignedInHandle();
std::string SignedInUserId();
bool SignedInVerified();
std::string SignedInVerifiedType();

} // namespace xsession

#endif
