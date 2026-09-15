// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XREWARDDIVIDEND_H
#define RAVEN_QT_XREWARDDIVIDEND_H

#include <QWidget>
#include <QString>

class WalletView;

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

/** Rewards → Asset dividends. Wraps requestsnapshot / distributereward. */
class XRewardDividend : public QWidget
{
    Q_OBJECT

public:
    explicit XRewardDividend(WalletView* walletView, QWidget* parent = 0);

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void onRefreshAssets();
    void onScheduleSnapshot();
    void onPay();

private:
    void applyTheme();
    QString rpc(const QString& method, const QStringList& args = QStringList()) const;
    void showRpcOutcome(const QString& raw, const QString& okPrefix);

    WalletView* walletView;
    QComboBox* targetCombo;
    QComboBox* payCombo;
    QLineEdit* amountEdit;
    QLineEdit* heightEdit;
    QLineEdit* excludeEdit;
    QPlainTextEdit* logEdit;
    QLabel* statusLabel;
};

#endif
