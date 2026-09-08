// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XHOME_H
#define RAVEN_QT_XHOME_H

#include <QWidget>
#include <QString>

class WalletModel;
class ClientModel;
class WalletView;
class XOAuth;

class QLabel;
class QLineEdit;
class QPushButton;

/** Primary X-Coin home: linked X identity, keys vs session, receive, send, assets, lottery. */
class XHome : public QWidget
{
    Q_OBJECT

public:
    explicit XHome(WalletView* walletView, QWidget* parent = 0);
    ~XHome();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);

Q_SIGNALS:
    void gotoReceive();
    void gotoSend();
    void gotoActivity();
    void gotoAssets();

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void onSignIn();
    void onSaveClientId();
    void onMockSignIn();
    void onAllowlistMe();
    void onClaim();
    void onIssueSub();
    void onIssueUnique();
    void onOAuthSuccess(const QString& username, const QString& userId);
    void onOAuthFailed(const QString& error);
    void onOAuthStatus(const QString& message);

private:
    void applyTheme();
    QString rpc(const QString& method, const QStringList& args = QStringList()) const;

    WalletView* walletView;
    ClientModel* clientModel;
    WalletModel* walletModel;
    XOAuth* oauth;

    QLabel* heroTitle;
    QLabel* peersLabel;
    QLabel* sessionLabel;
    QLabel* keysLabel;
    QLabel* lotteryLabel;
    QLabel* assetLabel;
    QLabel* balanceLabel;
    QLabel* statusLabel;
    QLineEdit* clientIdEdit;
    QLineEdit* subNameEdit;
    QLineEdit* uniqueNameEdit;
    QPushButton* signInBtn;
    QPushButton* mockBtn;
    QPushButton* claimBtn;
    QPushButton* allowlistBtn;
};

#endif // RAVEN_QT_XHOME_H
