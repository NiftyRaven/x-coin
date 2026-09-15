// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XREWARDSHARE_H
#define RAVEN_QT_XREWARDSHARE_H

#include <QWidget>
#include <QString>

class WalletView;

class QCheckBox;
class QLabel;
class QLineEdit;
class QTableWidget;

/** Rewards → Lottery share. Same hostshare RPCs as 1.0.14 Home. */
class XRewardShare : public QWidget
{
    Q_OBJECT

public:
    explicit XRewardShare(WalletView* walletView, QWidget* parent = 0);

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void onShareToggled(bool on);
    void onAddGuest();
    void onRemoveGuest();
    void onPercentEdited();

private:
    void applyTheme();
    QString rpc(const QString& method, const QStringList& args = QStringList()) const;
    void showRpcOutcome(const QString& raw, const QString& okPrefix);

    WalletView* walletView;
    QCheckBox* shareChk;
    QLineEdit* pctEdit;
    QLineEdit* handleEdit;
    QLabel* previewLabel;
    QLabel* statusLabel;
    QTableWidget* guestTable;
};

#endif
