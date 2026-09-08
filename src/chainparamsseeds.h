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
#ifdef __GNUC__
#define XCOIN_UNUSED_SEED __attribute__((unused))
#else
#define XCOIN_UNUSED_SEED
#endif
static XCOIN_UNUSED_SEED SeedSpec6 pnSeed6_main[] = {
    {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0}
};
static XCOIN_UNUSED_SEED SeedSpec6 pnSeed6_test[] = {
    {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0}
};
#endif // RAVEN_CHAINPARAMSSEEDS_H
