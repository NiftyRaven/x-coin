// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_XOAUTH_COOLDOWN_H
#define RAVEN_XOAUTH_COOLDOWN_H

#include <stdint.h>

/** Retry wait after a failed or abandoned Sign in. A successful sign-in does not stamp this. */
static const int64_t XOAUTH_COOLDOWN_MS = 60 * 1000;
/** How long the loopback listener stays up for a human X login. */
static const int64_t XOAUTH_LOGIN_WAIT_MS = 5 * 60 * 1000;

inline int64_t XOAuthCooldownRemainingMs(int64_t nowMs, int64_t lastMs, int64_t cooldownMs = XOAUTH_COOLDOWN_MS)
{
    if (lastMs <= 0 || nowMs < lastMs)
        return 0;
    const int64_t elapsed = nowMs - lastMs;
    if (elapsed >= cooldownMs)
        return 0;
    return cooldownMs - elapsed;
}

#endif
