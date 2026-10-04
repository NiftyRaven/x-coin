// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xcoinsend.h"

#include "coincontroldialog.h"
#include "hostshare.h"
#include "optionsmodel.h"
#include "ravenunits.h"
#include "walletmodel.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"

#include <QMessageBox>
#include <QWidget>

#include <vector>

namespace {

bool looksLikeHandle(const QString& s)
{
    QString t = s.trimmed();
    if (t.startsWith(QLatin1Char('@')))
        return true;
    if (t.size() >= 26 && t.startsWith(QLatin1Char('X')))
        return false;
    if (t.size() >= 1 && t.size() <= 32) {
        for (int i = 0; i < t.size(); ++i) {
            const QChar c = t.at(i);
            if (!c.isLetterOrNumber() && c != QLatin1Char('_'))
                return false;
        }
        return true;
    }
    return false;
}

QString statusText(const WalletModel::SendCoinsReturn& ret)
{
    switch (ret.status) {
    case WalletModel::InvalidAddress:
        return QStringLiteral("That address is not a valid X Coin address.");
    case WalletModel::InvalidAmount:
        return QStringLiteral("Enter an amount of XFER.");
    case WalletModel::AmountExceedsBalance:
        return QStringLiteral("That amount is more than the available balance.");
    case WalletModel::AmountWithFeeExceedsBalance:
        return QStringLiteral("The amount plus the fee is more than the available balance.");
    case WalletModel::DuplicateAddress:
        return QStringLiteral("That address is already in this send.");
    case WalletModel::AbsurdFee:
        return QStringLiteral("The fee for this send is too high.");
    case WalletModel::TransactionCommitFailed:
        return ret.reasonCommitFailed.isEmpty()
            ? QStringLiteral("The transaction was not accepted.")
            : ret.reasonCommitFailed;
    case WalletModel::TransactionCreationFailed:
        return QStringLiteral("Could not build that transaction.");
    case WalletModel::PaymentRequestExpired:
        return QStringLiteral("The payment request expired.");
    case WalletModel::SessionRequired:
        return QStringLiteral("Sign in before sending.");
    case WalletModel::OK:
        return QString();
    }
    return QStringLiteral("Send failed.");
}

} // namespace

namespace xcoinsend {

QString ResolveDestination(const QString& in, QString* displayName, QString* errOut)
{
    const QString t = in.trimmed();
    if (!looksLikeHandle(t))
        return t;
    std::string asset, addr, err;
    if (!hostshare::ResolveHolder(t.toStdString(), asset, addr, err) || addr.empty()) {
        if (errOut)
            *errOut = err.empty() ? QStringLiteral("That name has not claimed a root yet.") : QString::fromStdString(err);
        return QString();
    }
    if (displayName) {
        QString h = t;
        if (h.startsWith(QLatin1Char('@')))
            h = h.mid(1);
        *displayName = QLatin1Char('@') + h;
    }
    return QString::fromStdString(addr);
}

void SetCoinControlEnabled(OptionsModel* options, bool enabled)
{
    if (!options || options->getCoinControlFeatures() == enabled)
        return;
    options->setData(options->index(OptionsModel::CoinControlFeatures), enabled);
}

bool CoinControlEnabled(const WalletModel* model)
{
    return model && model->getOptionsModel() && model->getOptionsModel()->getCoinControlFeatures();
}

QString CoinSelectionText(bool enabled)
{
    if (!enabled)
        return QStringLiteral("Coins are chosen automatically.");
    if (!CoinControlDialog::coinControl || !CoinControlDialog::coinControl->HasSelected())
        return QStringLiteral("No coins chosen yet. Selection stays automatic until you pick.");
    std::vector<COutPoint> selected;
    CoinControlDialog::coinControl->ListSelected(selected);
    return QStringLiteral("%1 coin(s) selected for this send.").arg(static_cast<int>(selected.size()));
}

void ChooseCoins(QWidget* parent, const PlatformStyle* platformStyle, WalletModel* model)
{
    if (!model)
        return;
    CoinControlDialog dlg(platformStyle, parent);
    dlg.setModel(model);
    dlg.exec();
}

void ClearCoinSelection()
{
    if (CoinControlDialog::coinControl)
        CoinControlDialog::coinControl->UnSelectAll();
}

Outcome SendResolved(QWidget* parent, WalletModel* model, const QString& resolvedAddress,
                     const QString& displayName, const QString& amountText, bool coinControl)
{
    Outcome out;
    out.result = Failed;
    if (!model || !model->getOptionsModel()) {
        out.message = QStringLiteral("Open a wallet before sending.");
        return out;
    }
    CAmount nAmount = 0;
    const int unit = model->getOptionsModel()->getDisplayUnit();
    if (!RavenUnits::parse(unit, amountText.trimmed(), &nAmount) || nAmount <= 0) {
        out.message = QStringLiteral("Enter an amount of XFER.");
        return out;
    }
    if (!model->validateAddress(resolvedAddress)) {
        out.message = QStringLiteral("That address is not a valid X Coin address.");
        return out;
    }

    const QString who = displayName.isEmpty() ? resolvedAddress : (displayName + QStringLiteral("\n") + resolvedAddress);
    const QString shown = RavenUnits::formatWithUnit(unit, nAmount);
    if (QMessageBox::question(parent, QStringLiteral("X Coin"),
            QStringLiteral("Send %1 to\n%2?").arg(shown, who),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        out.result = Cancelled;
        return out;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {
        out.result = Cancelled;
        out.message = QStringLiteral("Unlock cancelled.");
        return out;
    }

    SendCoinsRecipient recipient(resolvedAddress, displayName, nAmount, QString());
    QList<SendCoinsRecipient> recipients;
    recipients.append(recipient);
    WalletModelTransaction draft(recipients);

    CCoinControl ctrl;
    if (coinControl && CoinControlDialog::coinControl)
        ctrl = *CoinControlDialog::coinControl;

    WalletModel::SendCoinsReturn prepared = model->prepareTransaction(draft, ctrl);
    if (prepared.status != WalletModel::OK) {
        out.message = statusText(prepared);
        if (out.message.isEmpty())
            out.message = QStringLiteral("Could not build that transaction.");
        return out;
    }

    WalletModel::SendCoinsReturn sent = model->sendCoins(draft);
    if (sent.status != WalletModel::OK) {
        out.message = statusText(sent);
        if (out.message.isEmpty())
            out.message = QStringLiteral("Send failed.");
        return out;
    }

    ClearCoinSelection();
    QString txid;
    if (draft.getTransaction() && draft.getTransaction()->tx)
        txid = QString::fromStdString(draft.getTransaction()->GetHash().GetHex());
    out.result = Sent;
    out.message = txid;
    return out;
}

} // namespace xcoinsend
