// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"
#include "arith_uint256.h"

#include <assert.h>
#include <cstdio>
#include "chainparamsseeds.h"

static void CheckGenesis(const char* net, const CBlock& genesis, const uint256& hash,
                         const char* expectHash, const char* expectMerkle)
{
    if (hash != uint256S(expectHash) || genesis.hashMerkleRoot != uint256S(expectMerkle)) {
        fprintf(stderr, "XCOIN_GENESIS %s nTime=%u hash=%s merkle=%s\n",
                net, genesis.nTime, hash.GetHex().c_str(), genesis.hashMerkleRoot.GetHex().c_str());
        fflush(stderr);
    }
    assert(hash == uint256S(expectHash));
    assert(genesis.hashMerkleRoot == uint256S(expectMerkle));
}

extern double algoHashTotal[16];
extern int algoHashHits[16];
