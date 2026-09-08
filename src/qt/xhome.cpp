// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xhome.h"

#include "assets/xaccount.h"
#include "chainparams.h"
#include "walletview.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "optionsmodel.h"
#include "ravenunits.h"
#include "xoauth.h"
#include "xsession.h"

#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <univalue.h>

static const char *kTheme =
    "QWidget#xhomeInner, QWidget { background: #000000; color: #ffffff; }"
    "QLabel#xhero { color: #ffffff; font-size: 42px; font-weight: 800; letter-spacing: 10px; }"
    "QLabel#xtag { color: #a1a1aa; font-size: 13px; }"
    "QLabel#xsection { color: #ffffff; font-size: 12px; font-weight: 800; letter-spacing: 3px; margin-top: 8px; }"
    "QLabel#xcard { background: #0a0a0a; border: 1px solid #27272a; border-radius: 0px; padding: 14px; color: #e4e4e7; font-size: 13px; }"
    "QLabel#xhint { color: #71717a; font-size: 12px; }"
    "QPushButton#xprimary { background: #ffffff; color: #000000; border: none; border-radius: 0px; padding: 10px 22px; font-weight: 700; font-size: 14px; }"
    "QPushButton#xprimary:hover { background: #e4e4e7; }"
    "QPushButton#xprimary:disabled { background: #27272a; color: #71717a; }"
    "QPushButton#xghost { background: #000000; color: #ffffff; border: 1px solid #52525b; border-radius: 0px; padding: 10px 18px; font-weight: 600; }"
    "QPushButton#xghost:hover { border-color: #ffffff; }"
    "QLineEdit { background: #0a0a0a; color: #ffffff; border: 1px solid #3f3f46; border-radius: 0px; padding: 10px 12px; selection-background-color: #ffffff; selection-color: #000000; }"
    "QLineEdit:focus { border-color: #ffffff; }";

XHome::XHome(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , clientModel(0)
    , walletModel(0)
    , oauth(new XOAuth(this))
{
    applyTheme();

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: #000000; border: none; }");

    QWidget* inner = new QWidget;
    inner->setObjectName("xhomeInner");
    QVBoxLayout* root = new QVBoxLayout(inner);
    root->setContentsMargins(28, 24, 28, 28);
    root->setSpacing(18);

    heroTitle = new QLabel("X");
    heroTitle->setObjectName("xhero");
    heroTitle->setAlignment(Qt::AlignHCenter);
    root->addWidget(heroTitle);

    QLabel* word = new QLabel("X-COIN");
    word->setObjectName("xsection");
    word->setAlignment(Qt::AlignHCenter);
    root->addWidget(word);

    QLabel* tag = new QLabel("Private test chain · Sign in with X · one root per handle");
    tag->setObjectName("xtag");
    tag->setAlignment(Qt::AlignHCenter);
    tag->setWordWrap(true);
    root->addWidget(tag);

    balanceLabel = new QLabel;
    balanceLabel->setObjectName("xcard");
    balanceLabel->setWordWrap(true);
    root->addWidget(balanceLabel);

    sessionLabel = new QLabel;
    sessionLabel->setObjectName("xcard");
    sessionLabel->setWordWrap(true);
    root->addWidget(sessionLabel);

    QHBoxLayout* authRow = new QHBoxLayout;
    signInBtn = new QPushButton("Sign in with X");
    signInBtn->setObjectName("xprimary");
    signInBtn->setMinimumHeight(48);
    signInBtn->setCursor(Qt::PointingHandCursor);
    authRow->addWidget(signInBtn);

    mockBtn = new QPushButton("Simulate sign-in (regtest)");
    mockBtn->setObjectName("xghost");
    mockBtn->setMinimumHeight(48);
    mockBtn->setCursor(Qt::PointingHandCursor);
    mockBtn->setVisible(GetParams().MineBlocksOnDemand());
    authRow->addWidget(mockBtn);
    root->addLayout(authRow);

    QLabel* credHint = new QLabel(
        "Operator: paste your X app Client ID. Callback is "
        "http://127.0.0.1:18791/callback — see docs/XSIGNIN.md. "
        "Nothing is hardcoded. Login never succeeds without a real token "
        "(or a -regtest mock of GET /2/users/me).");
    credHint->setObjectName("xhint");
    credHint->setWordWrap(true);
    root->addWidget(credHint);

    QHBoxLayout* credRow = new QHBoxLayout;
    clientIdEdit = new QLineEdit;
    clientIdEdit->setPlaceholderText("X OAuth Client ID");
    clientIdEdit->setText(XOAuth::ClientId());
    QPushButton* saveId = new QPushButton("Save Client ID");
    saveId->setObjectName("xghost");
    saveId->setCursor(Qt::PointingHandCursor);
    credRow->addWidget(clientIdEdit, 1);
    credRow->addWidget(saveId);
    root->addLayout(credRow);

    lotteryLabel = new QLabel;
    lotteryLabel->setObjectName("xcard");
    lotteryLabel->setWordWrap(true);
    root->addWidget(lotteryLabel);

    assetLabel = new QLabel;
    assetLabel->setObjectName("xcard");
    assetLabel->setWordWrap(true);
    root->addWidget(assetLabel);

    QHBoxLayout* claimRow = new QHBoxLayout;
    allowlistBtn = new QPushButton("Allowlist my handle");
    allowlistBtn->setObjectName("xghost");
    allowlistBtn->setMinimumHeight(44);
    allowlistBtn->setCursor(Qt::PointingHandCursor);
    claimBtn = new QPushButton("Claim my root asset");
    claimBtn->setObjectName("xprimary");
    claimBtn->setMinimumHeight(44);
    claimBtn->setCursor(Qt::PointingHandCursor);
    claimRow->addWidget(allowlistBtn);
    claimRow->addWidget(claimBtn);
    root->addLayout(claimRow);

    QLabel* issueHead = new QLabel("ISSUE");
    issueHead->setObjectName("xsection");
    root->addWidget(issueHead);

    QHBoxLayout* subRow = new QHBoxLayout;
    subNameEdit = new QLineEdit;
    subNameEdit->setPlaceholderText("sub name (e.g. pass)");
    QPushButton* subBtn = new QPushButton("Issue sub");
    subBtn->setObjectName("xghost");
    subBtn->setCursor(Qt::PointingHandCursor);
    subRow->addWidget(subNameEdit, 1);
    subRow->addWidget(subBtn);
    root->addLayout(subRow);

    QHBoxLayout* uniqRow = new QHBoxLayout;
    uniqueNameEdit = new QLineEdit;
    uniqueNameEdit->setPlaceholderText("unique name (e.g. ticket1)");
    QPushButton* uniqBtn = new QPushButton("Issue unique");
    uniqBtn->setObjectName("xghost");
    uniqBtn->setCursor(Qt::PointingHandCursor);
    uniqRow->addWidget(uniqueNameEdit, 1);
    uniqRow->addWidget(uniqBtn);
    root->addLayout(uniqRow);

    QLabel* moneyHead = new QLabel("WALLET");
    moneyHead->setObjectName("xsection");
    root->addWidget(moneyHead);

    QHBoxLayout* money = new QHBoxLayout;
    QPushButton* recv = new QPushButton("Receive");
    QPushButton* send = new QPushButton("Send");
    recv->setObjectName("xprimary");
    send->setObjectName("xprimary");
    recv->setMinimumHeight(44);
    send->setMinimumHeight(44);
    recv->setCursor(Qt::PointingHandCursor);
    send->setCursor(Qt::PointingHandCursor);
    money->addWidget(recv);
    money->addWidget(send);
    root->addLayout(money);

    statusLabel = new QLabel;
    statusLabel->setObjectName("xhint");
    statusLabel->setWordWrap(true);
    root->addWidget(statusLabel);

    root->addStretch(1);

    scroll->setWidget(inner);
    QVBoxLayout* wrap = new QVBoxLayout(this);
    wrap->setContentsMargins(0, 0, 0, 0);
    wrap->addWidget(scroll);

    connect(signInBtn, SIGNAL(clicked()), this, SLOT(onSignIn()));
    connect(mockBtn, SIGNAL(clicked()), this, SLOT(onMockSignIn()));
    connect(saveId, SIGNAL(clicked()), this, SLOT(onSaveClientId()));
    connect(allowlistBtn, SIGNAL(clicked()), this, SLOT(onAllowlistMe()));
    connect(claimBtn, SIGNAL(clicked()), this, SLOT(onClaim()));
    connect(subBtn, SIGNAL(clicked()), this, SLOT(onIssueSub()));
    connect(uniqBtn, SIGNAL(clicked()), this, SLOT(onIssueUnique()));
    connect(recv, SIGNAL(clicked()), this, SIGNAL(gotoReceive()));
    connect(send, SIGNAL(clicked()), this, SIGNAL(gotoSend()));
    connect(oauth, SIGNAL(signedIn(QString,QString)), this, SLOT(onOAuthSuccess(QString,QString)));
    connect(oauth, SIGNAL(failed(QString)), this, SLOT(onOAuthFailed(QString)));
    connect(oauth, SIGNAL(status(QString)), this, SLOT(onOAuthStatus(QString)));

    QTimer* t = new QTimer(this);
    connect(t, SIGNAL(timeout()), this, SLOT(refresh()));
    t->start(4000);

    refresh();
}

XHome::~XHome()
{
}

void XHome::applyTheme()
{
    setStyleSheet(kTheme);
}

void XHome::setClientModel(ClientModel* model)
{
    clientModel = model;
}

void XHome::setWalletModel(WalletModel* model)
{
    walletModel = model;
    refresh();
}

QString XHome::rpc(const QString& method, const QStringList& args) const
{
    if (!walletView) return QString();
    return walletView->callRpc(method, args);
}

void XHome::refresh()
{
    UniValue s;
    s.read(rpc("getxsession").toStdString());
    const bool signedIn = s.isObject() && s["signed_in"].isTrue();
    if (signedIn) {
        QString uid = QString::fromStdString(s["user_id"].getValStr());
        if (uid.isEmpty())
            uid = QString::fromStdString(s["id"].getValStr());
        sessionLabel->setText(QString("Signed in as @%1\nX user id %2")
            .arg(QString::fromStdString(s["username"].getValStr()))
            .arg(uid));
        signInBtn->setText("Signed in with X");
    } else {
        sessionLabel->setText("Not signed in.\nSign in with X is required to claim a root or enter the lottery. "
                              "Typing someone else's handle does nothing.");
        signInBtn->setText("Sign in with X");
    }

    if (walletModel) {
        const int unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : 0;
        balanceLabel->setText("Balance\n" + RavenUnits::formatWithUnit(unit, walletModel->getBalance()));
    } else {
        balanceLabel->setText("Balance\n(open or create a wallet from File)");
    }

    UniValue l;
    if (l.read(rpc("getlotteryinfo").toStdString()) && l.isObject()) {
        const bool elig = l["local_eligible"].isTrue();
        const QString handle = QString::fromStdString(l["local_xaccount"].getValStr());
        lotteryLabel->setText(QString("Lottery\nEligible: %1\nNext draw: block %2 (one block per minute slot)\nYour handle: %3\nActive nodes: %4")
            .arg(elig ? "yes" : "no")
            .arg(QString::fromStdString(l["next_draw_height"].getValStr().empty()
                                            ? l["height"].getValStr()
                                            : l["next_draw_height"].getValStr()))
            .arg(handle.isEmpty() ? QString("(sign in first)") : handle)
            .arg(QString::fromStdString(l["active_nodes"].getValStr())));
    } else {
        lotteryLabel->setText("Lottery\n(node starting…)");
    }

    QString rootName;
    if (signedIn) {
        std::string expect, err;
        if (MapXHandleToRootName(s["username"].getValStr(), expect, err)) {
            UniValue a;
            if (a.read(rpc("listmyassets").toStdString()) && a.isObject()) {
                if (a.exists(expect) || a.exists(expect + "!"))
                    rootName = QString::fromStdString(expect);
            }
        }
    }
    if (rootName.isEmpty())
        assetLabel->setText("My asset\nNo root yet. After you sign in, tap Allowlist my handle (operator) then Claim my root asset. "
                            "The node uses the signed-in username only — a typed foreign handle is rejected.");
    else
        assetLabel->setText("My asset\nRoot: " + rootName + "\nIssue a sub or unique below — no CLI.");

    claimBtn->setEnabled(signedIn && rootName.isEmpty());
    allowlistBtn->setEnabled(signedIn);
}

void XHome::onSignIn()
{
    statusLabel->setText("Opening X in your browser…");
    oauth->startLogin();
}

void XHome::onSaveClientId()
{
    const QString id = clientIdEdit->text().trimmed();
    QString err;
    if (!XOAuth::SaveClientId(id, err)) {
        QMessageBox::warning(this, "X-Coin", err.isEmpty()
            ? QString("Paste the Client ID from developer.x.com. Nothing is hardcoded.")
            : err);
        return;
    }
    statusLabel->setText("Saved xoauthclientid= in xcoin.conf. Sign in with X will use this Client ID.");
}

void XHome::onMockSignIn()
{
    bool ok = false;
    QString handle = QInputDialog::getText(this, "Regtest simulate",
        "Mock GET /2/users/me username (regtest only). This writes a session proof; it is not a live X login.",
        QLineEdit::Normal, "NFTRVN", &ok);
    if (!ok || handle.trimmed().isEmpty()) return;
    QString err;
    if (!oauth->mockSignIn(handle.trimmed(), err)) {
        QMessageBox::warning(this, "X-Coin", err);
        return;
    }
    statusLabel->setText("Regtest session injected from mock users/me.");
    refresh();
}

void XHome::onAllowlistMe()
{
    UniValue s;
    s.read(rpc("getxsession").toStdString());
    if (!s.isObject() || !s["signed_in"].isTrue()) {
        QMessageBox::warning(this, "X-Coin", "Sign in with X first. The allowlist uses the session username, not a typed field.");
        return;
    }
    statusLabel->setText(rpc("addxverified", QStringList() << QString::fromStdString(s["username"].getValStr())));
    refresh();
}

void XHome::onClaim()
{
    statusLabel->setText(rpc("linkxaccount"));
    refresh();
}

void XHome::onIssueSub()
{
    const QString leaf = subNameEdit->text().trimmed().toUpper();
    if (leaf.isEmpty()) {
        QMessageBox::information(this, "X-Coin", "Enter a sub name.");
        return;
    }
    UniValue s;
    s.read(rpc("getxsession").toStdString());
    if (!s.isObject() || !s["signed_in"].isTrue()) {
        QMessageBox::warning(this, "X-Coin", "Sign in with X first.");
        return;
    }
    std::string root, err;
    if (!MapXHandleToRootName(s["username"].getValStr(), root, err)) {
        QMessageBox::warning(this, "X-Coin", QString::fromStdString(err));
        return;
    }
    statusLabel->setText(rpc("issue", QStringList() << (QString::fromStdString(root) + "/" + leaf) << "1"));
    refresh();
}

void XHome::onIssueUnique()
{
    const QString leaf = uniqueNameEdit->text().trimmed().toUpper();
    if (leaf.isEmpty()) {
        QMessageBox::information(this, "X-Coin", "Enter a unique name.");
        return;
    }
    UniValue s;
    s.read(rpc("getxsession").toStdString());
    if (!s.isObject() || !s["signed_in"].isTrue()) {
        QMessageBox::warning(this, "X-Coin", "Sign in with X first.");
        return;
    }
    std::string root, err;
    if (!MapXHandleToRootName(s["username"].getValStr(), root, err)) {
        QMessageBox::warning(this, "X-Coin", QString::fromStdString(err));
        return;
    }
    const QString tags = QString("[\"%1\"]").arg(leaf);
    statusLabel->setText(rpc("issueunique", QStringList() << QString::fromStdString(root) << tags));
    refresh();
}

void XHome::onOAuthSuccess(const QString& username, const QString& userId)
{
    statusLabel->setText(QString("Signed in as @%1 (id %2). Identity is bound to this node.").arg(username).arg(userId));
    refresh();
}

void XHome::onOAuthFailed(const QString& error)
{
    statusLabel->setText(error);
    QMessageBox::warning(this, "Sign in with X", error);
}

void XHome::onOAuthStatus(const QString& message)
{
    statusLabel->setText(message);
}
