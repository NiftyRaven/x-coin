// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XSEND_H
#define RAVEN_QT_XSEND_H

#include <QWidget>
#include <QString>

class WalletModel;
class WalletView;

class QLabel;
class QLineEdit;
class QPushButton;

/** Send: paste address, amount, Send. No CLI. */
class XSend : public QWidget
{
    Q_OBJECT

public:
    explicit XSend(WalletView* walletView, QWidget* parent = 0);
    void setWalletModel(WalletModel* walletModel);
    void setAddress(const QString& addr);
    void refresh();

private Q_SLOTS:
    void onSend();
    void onPaste();

private:
    void applyTheme();
    QString rpc(const QString& method, const QStringList& args = QStringList()) const;

    WalletView* walletView;
    WalletModel* walletModel;
    QLineEdit* addrEdit;
    QLineEdit* amountEdit;
    QLabel* statusLabel;
    QLabel* balanceLabel;
};

#endif // RAVEN_QT_XSEND_H
