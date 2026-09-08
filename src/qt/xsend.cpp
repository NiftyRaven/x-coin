// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xsend.h"

#include "optionsmodel.h"
#include "ravenunits.h"
#include "walletmodel.h"
#include "walletview.h"
#include "xsession.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

XSend::XSend(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , walletModel(0)
{
    applyTheme();
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 36, 40, 36);
    root->setSpacing(18);

    QLabel* title = new QLabel("SEND");
    title->setObjectName("xsection");
    root->addWidget(title);

    tagLabel = new QLabel("Sign in with X first. Send requires the session proof that links this wallet to your X account.");
    tagLabel->setObjectName("xhint");
    tagLabel->setWordWrap(true);
    root->addWidget(tagLabel);

    balanceLabel = new QLabel;
    balanceLabel->setObjectName("xcard");
    root->addWidget(balanceLabel);

    addrEdit = new QLineEdit;
    addrEdit->setPlaceholderText("Paste destination address");
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
}

void XSend::applyTheme()
{
    setStyleSheet(
        "QWidget { background: #000000; color: #ffffff; }"
        "QLabel#xsection { color: #ffffff; font-size: 12px; font-weight: 800; letter-spacing: 3px; }"
        "QLabel#xhint { color: #71717a; font-size: 13px; }"
        "QLabel#xcard { background: #0a0a0a; border: 1px solid #27272a; padding: 14px; color: #e4e4e7; }"
        "QPushButton#xprimary { background: #ffffff; color: #000000; border: none; padding: 10px 22px; font-weight: 700; }"
        "QPushButton#xprimary:hover { background: #e4e4e7; }"
        "QPushButton#xghost { background: #000000; color: #ffffff; border: 1px solid #52525b; padding: 10px 18px; font-weight: 600; }"
        "QPushButton#xghost:hover { border-color: #ffffff; }"
        "QLineEdit { background: #0a0a0a; color: #ffffff; border: 1px solid #3f3f46; padding: 12px; selection-background-color: #ffffff; selection-color: #000000; }"
        "QLineEdit:focus { border-color: #ffffff; }");
}

void XSend::setWalletModel(WalletModel* model)
{
    walletModel = model;
    refresh();
}

void XSend::setAddress(const QString& addr)
{
    addrEdit->setText(addr);
}

void XSend::refresh()
{
    if (!xsession::HasValidSession()) {
        tagLabel->setText("This wallet is not linked yet. Sign in with X on Home. A typed handle cannot send.");
        balanceLabel->setText("Sign in with X required to send.\n"
                              "This wallet + this X session = you. Another user cannot send from inside your wallet.\n"
                              "The session proof is the identity gate. Spend only keys in this wallet.dat.");
        return;
    }
    const QString handle = QString::fromStdString(xsession::SignedInHandle());
    tagLabel->setText(QString("Sending from the wallet linked to @%1. "
                              "This wallet + this X session = you. Spend only keys in this wallet.dat. "
                              "Signing in as someone else does not import their coins. A typed handle cannot send as you.")
        .arg(handle));
    if (!walletModel) {
        balanceLabel->setText("Balance\n(open a wallet)");
        return;
    }
    const int unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : 0;
    balanceLabel->setText(QString("Available — wallet linked to @%1\n")
        .arg(handle) + RavenUnits::formatWithUnit(unit, walletModel->getBalance()));
}

QString XSend::rpc(const QString& method, const QStringList& args) const
{
    if (!walletView) return QString();
    return walletView->callRpc(method, args);
}

void XSend::onPaste()
{
    addrEdit->setText(QApplication::clipboard()->text().trimmed());
}

void XSend::onSend()
{
    if (!xsession::HasValidSession()) {
        QMessageBox::warning(this, "X-Coin", "Sign in with X required to send. Authentication proves it is you.");
        return;
    }
    const QString dest = addrEdit->text().trimmed();
    const QString amt = amountEdit->text().trimmed();
    if (dest.isEmpty() || amt.isEmpty()) {
        QMessageBox::information(this, "X-Coin", "Paste an address and an amount.");
        return;
    }
    const QString out = rpc("sendtoaddress", QStringList() << dest << amt);
    statusLabel->setText(out);
    refresh();
}
