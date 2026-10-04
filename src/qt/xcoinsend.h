// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XCOINSEND_H
#define RAVEN_QT_XCOINSEND_H

#include <QString>

class OptionsModel;
class PlatformStyle;
class QWidget;
class WalletModel;

/** Shared XFER send used by the Send page and the legacy balances page. */
namespace xcoinsend {

enum Result {
    Sent,
    Cancelled,
    Failed
};

struct Outcome {
    Result result;
    QString message;
};

QString ResolveDestination(const QString& in, QString* displayName, QString* errOut);

/** Existing coin-control option. Does not add a second selection rule. */
void SetCoinControlEnabled(OptionsModel* options, bool enabled);
bool CoinControlEnabled(WalletModel* model);
QString CoinSelectionText(bool enabled);

void ChooseCoins(QWidget* parent, const PlatformStyle* platformStyle, WalletModel* model);
void ClearCoinSelection();

/**
 * Confirm and send amountText XFER to an already-resolved address.
 * When coin control is on, spends CoinControlDialog's current selection.
 * When it is off, leaves automatic selection.
 */
Outcome SendResolved(QWidget* parent, WalletModel* model, const QString& resolvedAddress,
                     const QString& displayName, const QString& amountText, bool coinControl);

} // namespace xcoinsend

#endif
