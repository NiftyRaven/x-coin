// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xsend.h"

#include "optionsmodel.h"
#include "ravenunits.h"
#include "walletmodel.h"
#include "walletview.h"
#include "xcoinsend.h"
#include "xsession.h"
#include "xtheme.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

XSend::XSend(WalletView* walletViewIn, const PlatformStyle* platformStyleIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , platformStyle(platformStyleIn)
    , walletModel(0)
    , coinControlCheck(0)
    , chooseCoinsBtn(0)
    , coinSummary(0)
{
    applyTheme();
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 36, 40, 36);
    root->setSpacing(18);

    QLabel* title = new QLabel("SEND");
    title->setObjectName("xsection");
    root->addWidget(title);

    tagLabel = new QLabel("Paste an X address or type @handle.");
    tagLabel->setObjectName("xhint");
    tagLabel->setWordWrap(true);
    root->addWidget(tagLabel);

    balanceLabel = new QLabel;
    balanceLabel->setObjectName("xcard");
    root->addWidget(balanceLabel);

    addrEdit = new QLineEdit;
    addrEdit->setPlaceholderText("@handle or X address");
    root->addWidget(addrEdit);

    QHBoxLayout* pasteRow = new QHBoxLayout;
    QPushButton* paste = new QPushButton("Paste");
    paste->setObjectName("xghost");
    paste->setCursor(Qt::PointingHandCursor);
    pasteRow->addWidget(paste);
    pasteRow->addStretch(1);
    root->addLayout(pasteRow);

    amountEdit = new QLineEdit;
    amountEdit->setPlaceholderText("Amount (XFER)");
    root->addWidget(amountEdit);

    coinControlCheck = new QCheckBox("Coin control");
    coinControlCheck->setObjectName("xcheck");
    coinControlCheck->setCursor(Qt::PointingHandCursor);
    root->addWidget(coinControlCheck);

    chooseCoinsBtn = new QPushButton("Choose coins");
    chooseCoinsBtn->setObjectName("xghost");
    chooseCoinsBtn->setCursor(Qt::PointingHandCursor);
    chooseCoinsBtn->setVisible(false);
    root->addWidget(chooseCoinsBtn);

    coinSummary = new QLabel;
    coinSummary->setObjectName("xhint");
    coinSummary->setWordWrap(true);
    coinSummary->setVisible(false);
    root->addWidget(coinSummary);

    QPushButton* send = new QPushButton("Send");
    send->setObjectName("xprimary");
    send->setMinimumHeight(48);
    send->setCursor(Qt::PointingHandCursor);
    root->addWidget(send);

    statusLabel = new QLabel;
    statusLabel->setObjectName("xhint");
    statusLabel->setWordWrap(true);
    root->addWidget(statusLabel);
    root->addStretch(1);

    connect(paste, SIGNAL(clicked()), this, SLOT(onPaste()));
    connect(send, SIGNAL(clicked()), this, SLOT(onSend()));
    connect(coinControlCheck, SIGNAL(toggled(bool)), this, SLOT(onCoinControlToggled(bool)));
    connect(chooseCoinsBtn, SIGNAL(clicked()), this, SLOT(onChooseCoins()));
}

void XSend::applyTheme()
{
    ApplyXPageTheme(this);
}

void XSend::setWalletModel(WalletModel* model)
{
    walletModel = model;
    if (walletModel && walletModel->getOptionsModel()) {
        connect(walletModel->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)),
                this, SLOT(syncCoinControl()));
    }
    syncCoinControl();
    refresh();
}

void XSend::setAddress(const QString& addr)
{
    addrEdit->setText(addr);
}

void XSend::clearForm()
{
    addrEdit->clear();
    amountEdit->clear();
    if (statusLabel)
        statusLabel->clear();
    syncCoinControl();
}

void XSend::refresh()
{
    syncCoinControl();
    if (xsession::HasValidSession()) {
        const QString handle = QString::fromStdString(xsession::SignedInHandle());
        tagLabel->setText(QString("Sending from the wallet linked to @%1. Paste @handle or an X address.")
            .arg(handle));
    } else {
        tagLabel->setText("Paste an X address or type @handle.");
    }
    if (!walletModel) {
        balanceLabel->setText("Balance\n(open a wallet)");
        return;
    }
    const int unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : 0;
    if (xsession::HasValidSession()) {
        const QString handle = QString::fromStdString(xsession::SignedInHandle());
        balanceLabel->setText(QString("Available — wallet linked to @%1\n")
            .arg(handle) + RavenUnits::formatWithUnit(unit, walletModel->getBalance()));
    } else {
        balanceLabel->setText(QString("Available\n") + RavenUnits::formatWithUnit(unit, walletModel->getBalance()));
    }
}

void XSend::onPaste()
{
    addrEdit->setText(QApplication::clipboard()->text().trimmed());
}

void XSend::syncCoinControl()
{
    const bool on = xcoinsend::CoinControlEnabled(walletModel);
    if (coinControlCheck) {
        coinControlCheck->blockSignals(true);
        coinControlCheck->setChecked(on);
        coinControlCheck->blockSignals(false);
    }
    if (chooseCoinsBtn)
        chooseCoinsBtn->setVisible(on);
    if (coinSummary) {
        coinSummary->setVisible(on);
        coinSummary->setText(xcoinsend::CoinSelectionText(on));
    }
}

void XSend::onCoinControlToggled(bool checked)
{
    if (!walletModel || !walletModel->getOptionsModel()) {
        syncCoinControl();
        return;
    }
    xcoinsend::SetCoinControlEnabled(walletModel->getOptionsModel(), checked);
    syncCoinControl();
    if (checked)
        onChooseCoins();
}

void XSend::onChooseCoins()
{
    xcoinsend::ChooseCoins(this, platformStyle, walletModel);
    syncCoinControl();
}

void XSend::onSend()
{
    const QString dest = addrEdit->text().trimmed();
    const QString amt = amountEdit->text().trimmed();
    if (dest.isEmpty() || amt.isEmpty()) {
        QMessageBox::information(this, "X Coin", "Enter a name or address, and an amount.");
        return;
    }
    QString displayName;
    QString resolveErr;
    const QString resolved = xcoinsend::ResolveDestination(dest, &displayName, &resolveErr);
    if (resolved.isEmpty()) {
        const QString msg = resolveErr.isEmpty() ? QString("Could not resolve that name.") : resolveErr;
        statusLabel->setText(msg);
        QMessageBox::warning(this, "X Coin", msg);
        return;
    }
    const bool coinControl = xcoinsend::CoinControlEnabled(walletModel);
    const xcoinsend::Outcome outcome = xcoinsend::SendResolved(this, walletModel, resolved, displayName, amt, coinControl);
    if (outcome.result == xcoinsend::Cancelled)
        return;
    if (outcome.result == xcoinsend::Sent) {
        if (walletView)
            walletView->clearSendDrafts();
        else
            clearForm();
        statusLabel->setText(outcome.message.isEmpty()
            ? QStringLiteral("Sent.")
            : QStringLiteral("Sent.\n%1").arg(outcome.message));
        refresh();
        return;
    }
    const QString msg = outcome.message.isEmpty() ? QStringLiteral("Send failed.") : outcome.message;
    statusLabel->setText(msg);
    QMessageBox::warning(this, "X Coin", msg);
}
