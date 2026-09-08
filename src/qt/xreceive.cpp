// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xreceive.h"

#include "addresstablemodel.h"
#include "walletmodel.h"
#include "walletview.h"
#include "xsession.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

XReceive::XReceive(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , walletModel(0)
{
    applyTheme();
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 36, 40, 36);
    root->setSpacing(18);

    QLabel* title = new QLabel("RECEIVE");
    title->setObjectName("xsection");
    root->addWidget(title);

    tagLabel = new QLabel("Sign in with X first. Receive requires the session proof that links this wallet to your X account.");
    tagLabel->setObjectName("xhint");
    tagLabel->setWordWrap(true);
    root->addWidget(tagLabel);

    addressLabel = new QLabel("—");
    addressLabel->setObjectName("xaddr");
    addressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addressLabel->setWordWrap(true);
    root->addWidget(addressLabel);

    QHBoxLayout* row = new QHBoxLayout;
    QPushButton* copy = new QPushButton("Copy");
    QPushButton* neu = new QPushButton("New address");
    copy->setObjectName("xprimary");
    neu->setObjectName("xghost");
    copy->setMinimumHeight(44);
    neu->setMinimumHeight(44);
    copy->setCursor(Qt::PointingHandCursor);
    neu->setCursor(Qt::PointingHandCursor);
    row->addWidget(copy);
    row->addWidget(neu);
    root->addLayout(row);

    hintLabel = new QLabel;
    hintLabel->setObjectName("xhint");
    hintLabel->setWordWrap(true);
    root->addWidget(hintLabel);
    root->addStretch(1);

    connect(copy, SIGNAL(clicked()), this, SLOT(onCopy()));
    connect(neu, SIGNAL(clicked()), this, SLOT(onNewAddress()));
}

void XReceive::applyTheme()
{
    setStyleSheet(
        "QWidget { background: #000000; color: #ffffff; }"
        "QLabel#xsection { color: #ffffff; font-size: 12px; font-weight: 800; letter-spacing: 3px; }"
        "QLabel#xhint { color: #71717a; font-size: 13px; }"
        "QLabel#xaddr { background: #0a0a0a; border: 1px solid #ffffff; padding: 18px; font-size: 18px; font-family: monospace; color: #ffffff; }"
        "QPushButton#xprimary { background: #ffffff; color: #000000; border: none; padding: 10px 22px; font-weight: 700; }"
        "QPushButton#xprimary:hover { background: #e4e4e7; }"
        "QPushButton#xghost { background: #000000; color: #ffffff; border: 1px solid #52525b; padding: 10px 18px; font-weight: 600; }"
        "QPushButton#xghost:hover { border-color: #ffffff; }");
}

void XReceive::setWalletModel(WalletModel* model)
{
    walletModel = model;
    refresh();
}

void XReceive::refresh()
{
    currentAddress.clear();
    if (!xsession::HasValidSession()) {
        tagLabel->setText("This wallet is not linked yet. Sign in with X on Home. "
                          "A typed handle cannot create a receive address.");
        addressLabel->setText("(sign in with X to receive)");
        hintLabel->setText("Receive requires a private Sign in with X session on this computer. "
                           "This wallet + this X session = you. Another user cannot create a receive address in your wallet. "
                           "The 12-word seed still controls the keys; Sign in with X is the identity gate.");
        return;
    }
    const QString handle = QString::fromStdString(xsession::SignedInHandle());
    tagLabel->setText(QString("Receiving into the wallet linked to @%1.")
        .arg(handle));
    if (!walletModel || !walletModel->getAddressTableModel()) {
        addressLabel->setText("(open a wallet)");
        return;
    }
    AddressTableModel* m = walletModel->getAddressTableModel();
    const QModelIndex parent;
    for (int r = 0; r < m->rowCount(parent); ++r) {
        if (m->index(r, 0, parent).data(AddressTableModel::TypeRole).toString() == AddressTableModel::Receive) {
            currentAddress = m->index(r, AddressTableModel::Address, parent).data().toString();
            break;
        }
    }
    if (currentAddress.isEmpty())
        currentAddress = m->addRow(AddressTableModel::Receive, "Receive", "");
    addressLabel->setText(currentAddress.isEmpty() ? QString("(could not create address)") : currentAddress);
    hintLabel->setText(QString("This address is yours because this wallet.dat holds the keys and this node holds a "
                              "private Sign in with X session for @%1. "
                              "Another user cannot receive into your wallet by typing your handle. Copy and send this to the payer.")
        .arg(QString::fromStdString(xsession::SignedInHandle())));
}

void XReceive::onCopy()
{
    if (currentAddress.isEmpty()) {
        refresh();
    }
    if (currentAddress.isEmpty()) {
        QMessageBox::warning(this, "X-Coin", "Sign in with X first, then open a wallet.");
        return;
    }
    QApplication::clipboard()->setText(currentAddress);
    hintLabel->setText("Copied.");
}

void XReceive::onNewAddress()
{
    if (!xsession::HasValidSession()) {
        QMessageBox::warning(this, "X-Coin", "Sign in with X required to receive.");
        return;
    }
    if (!walletModel || !walletModel->getAddressTableModel()) return;
    currentAddress = walletModel->getAddressTableModel()->addRow(AddressTableModel::Receive, "Receive", "");
    addressLabel->setText(currentAddress);
    hintLabel->setText("New address ready.");
}
