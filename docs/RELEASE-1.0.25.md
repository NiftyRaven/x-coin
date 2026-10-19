# X Coin 1.0.25

Heavy wallet only. Light is unchanged. Consensus unchanged. Block validity, CheckTransaction, tx_verify.cpp, and ConnectBlock are the same as 1.0.24.

Sign in with X in the same session was saving the handle and then dropping the access token before Claim could use it. The browser callback also closed too soon for a real login.

The wallet now keeps that token until you close it. It listens on 127.0.0.1:18791 for about 5 minutes and shows that it is waiting. A successful sign-in does not start a cooldown. Claim my name uses the token already in memory. A failed or abandoned sign-in waits 1 minute, and the button shows the seconds left.

Coins, Launch, and existing names are unaffected. Older wallets still send and receive.

Package with `package-xcoin-1.0.25.yml`. Do not deploy from this change.
