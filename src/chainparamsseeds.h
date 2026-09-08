#ifndef RAVEN_CHAINPARAMSSEEDS_H
#define RAVEN_CHAINPARAMSSEEDS_H
/**
 * Fixed-seed arrays are unused. X Coin is a private mesh: join with
 * addnode / seednode (P2P 38443). Do not restore imported upstream IPs
 * and do not publish public DNS seeds from this tree.
 *
 * chainparams.cpp calls vSeeds.clear() / vFixedSeeds.clear() on every
 * network. The sentinels exist only so this header still compiles.
 */
static SeedSpec6 pnSeed6_main[] = {
    {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0}
};
static SeedSpec6 pnSeed6_test[] = {
    {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0}
};
#endif // RAVEN_CHAINPARAMSSEEDS_H
