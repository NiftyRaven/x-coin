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
class QLineEdit;
class QPushButton;
class QTableWidget;

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
    void onUseName();
    void onInvoiceClicked();

private:
    void applyTheme();
    void setQr(const QString& payload);
    void fillInvoiceTable();
    void showAddress(const QString& addr, const QString& why);
    QString nameAddress() const;
    QString nextInvoiceLabel() const;
    void reportAddRowError();

    WalletView* walletView;
    WalletModel* walletModel;
    QLabel* handleLabel;
    QLabel* tagLabel;
    QLabel* qrLabel;
    QLabel* addressLabel;
    QLabel* hintLabel;
    QLineEdit* labelEdit;
    QPushButton* useNameBtn;
    QTableWidget* invoiceTable;
    QString currentAddress;
    QString selectedAddress;
    QString nameCardAddress;
};

#endif
