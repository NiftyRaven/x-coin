// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XSEND_H
#define RAVEN_QT_XSEND_H

#include <QWidget>
#include <QString>

class PlatformStyle;
class WalletModel;
class WalletView;

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

/** Send: @handle or X address, amount, confirm, Send. */
class XSend : public QWidget
{
    Q_OBJECT

public:
    explicit XSend(WalletView* walletView, const PlatformStyle* platformStyle, QWidget* parent = 0);
    void setWalletModel(WalletModel* walletModel);
    void setAddress(const QString& addr);
    void refresh();
    /** Drop recipient, amount, and coin-control draft. */
    void clearForm();

private Q_SLOTS:
    void onSend();
    void onPaste();
    void onChooseCoins();
    void onCoinControlToggled(bool checked);
    void syncCoinControl();

private:
    void applyTheme();

    WalletView* walletView;
    const PlatformStyle* platformStyle;
    WalletModel* walletModel;
    QLineEdit* addrEdit;
    QLineEdit* amountEdit;
    QCheckBox* coinControlCheck;
    QPushButton* chooseCoinsBtn;
    QLabel* coinSummary;
    QLabel* tagLabel;
    QLabel* statusLabel;
    QLabel* balanceLabel;
};

#endif // RAVEN_QT_XSEND_H
