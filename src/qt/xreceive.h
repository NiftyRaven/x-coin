// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XRECEIVE_H
#define RAVEN_QT_XRECEIVE_H

#include <QWidget>
#include <QString>

class WalletModel;
class WalletView;

class QLabel;
class QPushButton;

/** Receive: show one address and a Copy button. No CLI. */
class XReceive : public QWidget
{
    Q_OBJECT

public:
    explicit XReceive(WalletView* walletView, QWidget* parent = 0);
    void setWalletModel(WalletModel* walletModel);
    void refresh();

private Q_SLOTS:
    void onCopy();
    void onNewAddress();

private:
    void applyTheme();

    WalletView* walletView;
    WalletModel* walletModel;
    QLabel* tagLabel;
    QLabel* addressLabel;
    QLabel* hintLabel;
    QString currentAddress;
};

#endif // RAVEN_QT_XRECEIVE_H
