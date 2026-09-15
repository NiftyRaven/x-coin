// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_XMARKET_H
#define RAVEN_XMARKET_H

#include "amount.h"
#include "primitives/transaction.h"
#include "uint256.h"

#include <string>
#include <vector>

class CConnman;
class CDataStream;
class CNode;

/**
 * Same-chain asset marketplace. Not a consensus change and not a server.
 *
 * A listing is a 1-in / 1-out partial transaction:
 *   vin[0]  = one asset UTXO
 *   vout[0] = price in XFER to the seller
 *   signed SIGHASH_SINGLE | ANYONECANPAY
 *
 * Peers gossip it as `xord`. Old nodes ignore unknown commands.
 * A buyer adds XFER inputs, XFER change, and an asset output to themselves,
 * signs the rest, and broadcasts. The seller signature stays valid.
 */
namespace xmarket {

static const size_t MAX_ASKS = 200;
static const unsigned int MAX_ASK_BYTES = 20000;
static const size_t MAX_PUSH_ON_CONNECT = 50;

int ProcessXord(CNode* pfrom, CDataStream& vRecv, CConnman& connman);
void PushBook(CNode* pto, CConnman& connman);
void RelayAsk(const CTransaction& tx, CConnman& connman, CNode* pfrom = nullptr);

bool ValidateAsk(const CTransaction& tx, std::string& strAsset, CAmount& nAmount, CAmount& nPrice, std::string& err);
bool AcceptAsk(const CTransaction& tx, std::string& err);
void PruneSpent();
void RemoveAsk(const uint256& hash);
CTransactionRef GetAsk(const uint256& hash);
std::vector<CTransactionRef> ListAsks();

} // namespace xmarket

#endif
