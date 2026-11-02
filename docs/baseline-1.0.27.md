# Phase 0 baseline — X Coin 1.0.27 (recorded 2026-11-02)

Method: official release `X-Coin-1.0.27-Linux-x86_64.tar.gz` (SHA256SUMS verified), **fresh datadir**, `-disablewallet -listen=0`, synced from genesis over P2P on an isolated, non-production box. No VPS node or wallet was touched.

| Field | Value |
|---|---|
| Height | 74223 |
| bestblockhash | cf16cbd6bb8f8adadde5885fae26883d7279fca6b0b9552f2035a07ae8060871 |
| getblockhash 0 | 7c790cdb7a233c020cb71346082709a185eb6577643a327349112d425a712beb |
| getblockhash 1000 | 50b1e6fd94835bdd42a9bb00f6f615978896eeb9f1e0c7e61c2f46137d6c89fc |
| getblockhash 50000 | 3a7fbe79a8fcacf4809e439613b9dde4ee1c533e4591ff75f5a4b6775dc58e49 |
| gettxoutsetinfo.transactions | 50158 |
| txouts | 53037 |
| bogosize | 4452943 |
| hash_serialized_2 | b87b1e37c18128b1422b0ad7166fe1b1cc9c61d46299751654e6533477c5fa5b |
| total_amount | 371115000.00000000 (= 74223 × 5000) |

Gate for each later phase: a new build syncs from genesis with a fresh datadir and, **at height 74223**, must return the same hashes and `hash_serialized_2` (compare with `getblockhash 74223`, and use `gettxoutsetinfo` at that height on a node stopped there, e.g. with `-stopatheight=74224`).

Unit/functional test baseline: **not yet recorded**. The analysis box could not install build dependencies (apt has no package sources for autoconf/bdb/zmq), so this must run on a build machine. TODO before Phase 2.
