# X Coin 1.0.24

Heavy wallet only. Light is unchanged. Consensus unchanged. Block validity, CheckTransaction, tx_verify.cpp, and ConnectBlock are the same as 1.0.23.

After a restart, crash, or forced quit, the wallet can still show your X handle from the saved session. The X access token stays in memory for this run only, so it is empty after the process starts again.

Sign in with X to claim. The Sign in with X button stays on and runs the full sign-in. Claim starts that sign-in when the token is missing, then claims the name once the token is set. A new sign-in replaces the restored session.

Coins, Launch, and existing names are unaffected. Older wallets still send and receive.

Package with the workflow_dispatch workflow `package-xcoin-1.0.24.yml`. Do not deploy from this change.
