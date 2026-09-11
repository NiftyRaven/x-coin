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

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

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
    void gotoCreateSub(const QString& leaf);
    void gotoCreateUnique(const QString& leaf);

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void onSignIn();
    void onReLink();
    void onMockSignIn();
    void onAllowlistMe();
    void onClaim();
    void onIssueSub();
    void onIssueUnique();
    void onOAuthSuccess(const QString& username, const QString& userId);
    void onOAuthFailed(const QString& error);
    void onOAuthStatus(const QString& message);
    void onShareNodeToggled(bool on);
    void onCopyNodeAddress();
    void onReleaseNotes();
    void onReleaseLater();

private:
    void applyTheme();
    QString rpc(const QString& method, const QStringList& args = QStringList()) const;
    void showRpcOutcome(const QString& raw, const QString& okPrefix);
    QString localListenEndpoint() const;
    void fillNodeShareWidgets();
    void applyNodeShareVisibility(bool on);
    void applyAuthButtons(bool signedIn);

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
    QLineEdit* subNameEdit;
    QLineEdit* uniqueNameEdit;
    QLineEdit* nodeIpEdit;
    QPushButton* signInBtn;
    QPushButton* reLinkBtn;
    QPushButton* mockBtn;
    QPushButton* claimBtn;
    QPushButton* allowlistBtn;
    QPushButton* copyNodeBtn;
    QCheckBox* shareNodeChk;
    QWidget* nodeSharePanel;
    QLabel* nodeEndpointLabel;
    QLabel* nodeStatusLabel;
    QWidget* releasePanel;
    QLabel* releaseLabel;
    QPushButton* releaseNotesBtn;
    QPushButton* releaseLaterBtn;
};

#endif // RAVEN_QT_XHOME_H
