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
#include "net.h"
#include "util.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHostInfo>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <univalue.h>

static const char *kTheme =
    "QWidget#xhomeInner, QWidget { background: #000000; color: #ffffff; }"
    "QLabel#xhero { color: #ffffff; font-size: 40px; font-weight: 800; letter-spacing: 8px; }"
    "QLabel#xtag { color: #a1a1aa; font-size: 13px; }"
    "QLabel#xsection { color: #ffffff; font-size: 12px; font-weight: 800; letter-spacing: 3px; margin-top: 8px; }"
    "QLabel#xcard { background: #0a0a0a; border: 1px solid #27272a; border-radius: 0px; padding: 14px; color: #e4e4e7; font-size: 13px; }"
    "QLabel#xbalance { background: #0a0a0a; border: 1px solid #ffffff; border-radius: 0px; padding: 18px; color: #ffffff; font-size: 22px; font-weight: 700; }"
    "QLabel#xlotteryok { background: #0a0a0a; border: 1px solid #ffffff; border-radius: 0px; padding: 16px; color: #ffffff; font-size: 13px; }"
    "QLabel#xlotteryno { background: #0a0a0a; border: 1px solid #3f3f46; border-radius: 0px; padding: 16px; color: #a1a1aa; font-size: 13px; }"
    "QLabel#xlinked { background: #0a0a0a; border: 1px solid #ffffff; border-radius: 0px; padding: 16px; color: #ffffff; font-size: 13px; }"
    "QLabel#xunlinked { background: #0a0a0a; border: 1px solid #3f3f46; border-radius: 0px; padding: 16px; color: #a1a1aa; font-size: 13px; }"
    "QLabel#xhint { color: #71717a; font-size: 12px; }"
    "QLabel#xstatuserr { color: #fca5a5; font-size: 12px; }"
    "QPushButton#xprimary { background: #ffffff; color: #000000; border: none; border-radius: 0px; padding: 10px 22px; font-weight: 700; font-size: 14px; }"
    "QPushButton#xprimary:hover { background: #e4e4e7; }"
    "QPushButton#xprimary:disabled { background: #27272a; color: #71717a; }"
    "QPushButton#xghost { background: #000000; color: #ffffff; border: 1px solid #52525b; border-radius: 0px; padding: 10px 18px; font-weight: 600; }"
    "QPushButton#xghost:hover { border-color: #ffffff; }"
    "QCheckBox#xcheck { color: #a1a1aa; font-size: 13px; spacing: 8px; }"
    "QCheckBox#xcheck:hover { color: #ffffff; }"
    "QCheckBox#xcheck::indicator { width: 16px; height: 16px; border: 1px solid #52525b; background: #000000; }"
    "QCheckBox#xcheck::indicator:checked { background: #ffffff; border-color: #ffffff; }"
    "QLineEdit { background: #0a0a0a; color: #ffffff; border: 1px solid #3f3f46; border-radius: 0px; padding: 10px 12px; selection-background-color: #ffffff; selection-color: #000000; }"
    "QLineEdit:focus { border-color: #ffffff; }";

XHome::XHome(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , clientModel(0)
    , walletModel(0)
    , oauth(new XOAuth(this))
    , nodeIpEdit(0)
    , signInBtn(0)
    , reLinkBtn(0)
    , copyNodeBtn(0)
    , shareNodeChk(0)
    , nodeSharePanel(0)
    , nodeEndpointLabel(0)
    , nodeStatusLabel(0)
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

    QLabel* tag = new QLabel("Private · Sign in with X · fair lottery");
    tag->setObjectName("xtag");
    tag->setAlignment(Qt::AlignHCenter);
    tag->setWordWrap(true);
    root->addWidget(tag);

    // Count only. Never list other people's addresses here — every xcoin-qt is already a node.
    // A user's node IP is their choice to provide. Off by default; never show automatically.
    // New wallets join when the package (or datadir) xcoin.conf has addnode=host:38443.
    peersLabel = new QLabel("Starting this node…");
    peersLabel->setObjectName("xtag");
    peersLabel->setAlignment(Qt::AlignHCenter);
    peersLabel->setWordWrap(true);
    root->addWidget(peersLabel);

    balanceLabel = new QLabel;
    balanceLabel->setObjectName("xbalance");
    balanceLabel->setWordWrap(true);
    root->addWidget(balanceLabel);

    QLabel* identHead = new QLabel("THIS WALLET");
    identHead->setObjectName("xsection");
    root->addWidget(identHead);

    sessionLabel = new QLabel;
    sessionLabel->setObjectName("xunlinked");
    sessionLabel->setWordWrap(true);
    root->addWidget(sessionLabel);

    keysLabel = new QLabel;
    keysLabel->setObjectName("xcard");
    keysLabel->setWordWrap(true);
    keysLabel->setText(
        "12-word seed restores this wallet. Sign in with X stays on this computer — "
        "login tokens are never saved and never sent to peers. Spend only keys in this wallet.dat.");
    root->addWidget(keysLabel);

    QHBoxLayout* authRow = new QHBoxLayout;
    signInBtn = new QPushButton("Sign in with X");
    signInBtn->setObjectName("xprimary");
    signInBtn->setMinimumHeight(48);
    signInBtn->setCursor(Qt::PointingHandCursor);
    authRow->addWidget(signInBtn);

    reLinkBtn = new QPushButton("Re-link");
    reLinkBtn->setObjectName("xghost");
    reLinkBtn->setMinimumHeight(48);
    reLinkBtn->setCursor(Qt::PointingHandCursor);
    reLinkBtn->setVisible(false);
    authRow->addWidget(reLinkBtn);

    mockBtn = new QPushButton("Simulate sign-in (regtest)");
    mockBtn->setObjectName("xghost");
    mockBtn->setMinimumHeight(48);
    mockBtn->setCursor(Qt::PointingHandCursor);
    mockBtn->setVisible(GetParams().MineBlocksOnDemand());
    authRow->addWidget(mockBtn);
    root->addLayout(authRow);

    QLabel* credHint = new QLabel(
        "Sign in with X opens your browser. Come back here when it finishes. "
        "There is no Client ID field here. The operator bakes xoauthclientid= in the "
        "xcoin.conf in this wallet folder (callback http://127.0.0.1:18791/callback). "
        "A typed handle cannot claim verified status.");
    credHint->setObjectName("xhint");
    credHint->setWordWrap(true);
    root->addWidget(credHint);

    lotteryLabel = new QLabel;
    lotteryLabel->setObjectName("xlotteryno");
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

    QLabel* issueHead = new QLabel("ASSETS");
    issueHead->setObjectName("xsection");
    root->addWidget(issueHead);

    QLabel* issueHint = new QLabel(
        "Create sub and unique on the Assets tab (quantity, units, IPFS/txid, reissuable). "
        "Unique quantity is 1. The root is Sign-in Claim only — you cannot create a main asset.");
    issueHint->setObjectName("xhint");
    issueHint->setWordWrap(true);
    root->addWidget(issueHint);

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
    QPushButton* activity = new QPushButton("Activity");
    QPushButton* assets = new QPushButton("Transfer assets");
    recv->setObjectName("xprimary");
    send->setObjectName("xprimary");
    activity->setObjectName("xghost");
    assets->setObjectName("xghost");
    recv->setMinimumHeight(44);
    send->setMinimumHeight(44);
    activity->setMinimumHeight(44);
    assets->setMinimumHeight(44);
    recv->setCursor(Qt::PointingHandCursor);
    send->setCursor(Qt::PointingHandCursor);
    activity->setCursor(Qt::PointingHandCursor);
    assets->setCursor(Qt::PointingHandCursor);
    money->addWidget(recv);
    money->addWidget(send);
    money->addWidget(activity);
    money->addWidget(assets);
    root->addLayout(money);

    QLabel* moneyHint = new QLabel(
        "Receive, Send, Activity, and Assets are also in the left menu. Transfer spends only keys in this wallet.dat.");
    moneyHint->setObjectName("xhint");
    moneyHint->setWordWrap(true);
    root->addWidget(moneyHint);

    QLabel* nodeHead = new QLabel("MY NODE");
    nodeHead->setObjectName("xsection");
    root->addWidget(nodeHead);

    nodeStatusLabel = new QLabel;
    nodeStatusLabel->setObjectName("xcard");
    nodeStatusLabel->setWordWrap(true);
    root->addWidget(nodeStatusLabel);

    shareNodeChk = new QCheckBox("Provide my node IP");
    shareNodeChk->setObjectName("xcheck");
    shareNodeChk->setCursor(Qt::PointingHandCursor);
    shareNodeChk->setToolTip("Off by default. Turn on to see the host:38443 address others must put in the download as addnode=. Other people's IPs are never listed.");
    root->addWidget(shareNodeChk);

    nodeSharePanel = new QWidget;
    QVBoxLayout* nodeLay = new QVBoxLayout(nodeSharePanel);
    nodeLay->setContentsMargins(0, 0, 0, 0);
    nodeLay->setSpacing(10);

    QLabel* nodeHint = new QLabel(
        "This is the address others must use. They do not find you through GitHub. "
        "Before you open the repo, put addnode=<host:port> in the public Windows and Linux "
        "package xcoin.conf. Other people's IPs are never listed.");
    nodeHint->setObjectName("xhint");
    nodeHint->setWordWrap(true);
    nodeLay->addWidget(nodeHint);

    nodeEndpointLabel = new QLabel;
    nodeEndpointLabel->setObjectName("xcard");
    nodeEndpointLabel->setWordWrap(true);
    nodeEndpointLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    nodeLay->addWidget(nodeEndpointLabel);

    QHBoxLayout* giveRow = new QHBoxLayout;
    nodeIpEdit = new QLineEdit;
    nodeIpEdit->setPlaceholderText("host:38443 — type the address others should add, if the detected one is wrong");
    copyNodeBtn = new QPushButton("Copy addnode line");
    copyNodeBtn->setObjectName("xghost");
    copyNodeBtn->setCursor(Qt::PointingHandCursor);
    giveRow->addWidget(nodeIpEdit, 1);
    giveRow->addWidget(copyNodeBtn);
    nodeLay->addLayout(giveRow);
    root->addWidget(nodeSharePanel);

    QSettings shareSettings;
    shareNodeChk->setChecked(shareSettings.value("xhomeShareNodeIp", false).toBool());
    applyNodeShareVisibility(shareNodeChk->isChecked());

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
    connect(reLinkBtn, SIGNAL(clicked()), this, SLOT(onReLink()));
    connect(mockBtn, SIGNAL(clicked()), this, SLOT(onMockSignIn()));
    connect(allowlistBtn, SIGNAL(clicked()), this, SLOT(onAllowlistMe()));
    connect(claimBtn, SIGNAL(clicked()), this, SLOT(onClaim()));
    connect(subBtn, SIGNAL(clicked()), this, SLOT(onIssueSub()));
    connect(uniqBtn, SIGNAL(clicked()), this, SLOT(onIssueUnique()));
    connect(recv, SIGNAL(clicked()), this, SIGNAL(gotoReceive()));
    connect(send, SIGNAL(clicked()), this, SIGNAL(gotoSend()));
    connect(activity, SIGNAL(clicked()), this, SIGNAL(gotoActivity()));
    connect(assets, SIGNAL(clicked()), this, SIGNAL(gotoAssets()));
    connect(oauth, SIGNAL(signedIn(QString,QString)), this, SLOT(onOAuthSuccess(QString,QString)));
    connect(oauth, SIGNAL(failed(QString)), this, SLOT(onOAuthFailed(QString)));
    connect(oauth, SIGNAL(status(QString)), this, SLOT(onOAuthStatus(QString)));
    connect(shareNodeChk, SIGNAL(toggled(bool)), this, SLOT(onShareNodeToggled(bool)));
    connect(copyNodeBtn, SIGNAL(clicked()), this, SLOT(onCopyNodeAddress()));

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
    if (clientModel)
        disconnect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(refresh()));
    clientModel = model;
    if (clientModel)
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(refresh()));
    refresh();
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

void XHome::showRpcOutcome(const QString& raw, const QString& okPrefix)
{
    bool ok = false;
    const QString text = WalletView::humanRpc(raw, &ok);
    statusLabel->setObjectName(ok ? "xhint" : "xstatuserr");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
    if (ok) {
        if (okPrefix.isEmpty())
            statusLabel->setText(text);
        else if (text.isEmpty() || text.startsWith("{") || text.startsWith("["))
            statusLabel->setText(okPrefix);
        else
            statusLabel->setText(okPrefix + " — " + text);
        return;
    }
    const QString msg = text.isEmpty() ? QStringLiteral("Request failed.") : text;
    statusLabel->setText(msg);
    QMessageBox::warning(this, "X-Coin", msg);
}

void XHome::applyAuthButtons(bool signedIn)
{
    if (!signInBtn || !reLinkBtn)
        return;
    const qint64 left = XOAuth::cooldownRemainingMs();
    const bool cooling = left > 0;
    qint64 mins = cooling ? (left + 59999) / 60000 : 0;
    if (cooling && mins < 1)
        mins = 1;

    if (signedIn) {
        signInBtn->setVisible(false);
        signInBtn->setEnabled(false);
        reLinkBtn->setVisible(true);
        reLinkBtn->setEnabled(!cooling);
        reLinkBtn->setText(cooling ? QString("Re-link in ~%1 min").arg(mins) : QString("Re-link"));
    } else {
        reLinkBtn->setVisible(false);
        reLinkBtn->setEnabled(false);
        reLinkBtn->setText("Re-link");
        signInBtn->setVisible(true);
        signInBtn->setEnabled(!cooling);
        signInBtn->setText(cooling ? QString("Sign in with X (~%1 min)").arg(mins) : QString("Sign in with X"));
    }
}

void XHome::refresh()
{
    // Local session proof only. Do not call GET /2/users/me on Home paint.
    // Linked / Verified refresh from X only after a successful OAuth or Re-link.
    UniValue s;
    s.read(rpc("getxsession").toStdString());
    const bool signedIn = s.isObject() && (s["signed_in"].isTrue() || s["linked"].isTrue());
    if (signedIn) {
        const QString handle = QString::fromStdString(s["username"].getValStr());
        // Local xsession.json from the last successful users/me / Re-link. Never invent true.
        const bool xVerified = s["x_verified"].isTrue() || s["verified"].isTrue();
        const QString vtype = QString::fromStdString(s["verified_type"].getValStr());
        QString verifiedLine;
        if (xVerified) {
            verifiedLine = QString("X Verified: yes (%1). This running wallet cannot be excluded from the lottery.")
                .arg(vtype.isEmpty() ? QString("blue check") : vtype + " check");
        } else {
            verifiedLine = QString(
                "X Verified: no. Lottery chance is zero. Allowlist is an invite list — it does not make @%1 verified.")
                .arg(handle);
        }
        sessionLabel->setObjectName("xlinked");
        sessionLabel->setText(QString(
            "Linked to @%1\n"
            "%2\n"
            "This wallet + this X session = you. Tokens stay off disk and off the wire.")
            .arg(handle)
            .arg(verifiedLine));
    } else {
        sessionLabel->setObjectName("xunlinked");
        sessionLabel->setText(
            "Not linked yet. Sign in with X to bind this wallet to your account.\n"
            "Send and receive work without Sign-in. Sign in to claim the free root. A typed handle cannot steal this.\n"
            "Lottery is X Verified only (blue / business / government check). Unverified has zero chance.");
    }
    applyAuthButtons(signedIn);
    sessionLabel->style()->unpolish(sessionLabel);
    sessionLabel->style()->polish(sessionLabel);

    if (walletModel) {
        const int unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : 0;
        balanceLabel->setText("Balance\n" + RavenUnits::formatWithUnit(unit, walletModel->getBalance()));
    } else {
        balanceLabel->setText("Balance\n(open or create a wallet from File)");
    }

    const bool hasJoin = !gArgs.GetArgs("-addnode").empty()
        || !gArgs.GetArgs("-seednode").empty()
        || (!gArgs.GetArgs("-connect").empty() && gArgs.GetArg("-connect", "0") != "0");
    int listenPort = GetListenPort();
    if (listenPort <= 0)
        listenPort = GetParams().GetDefaultPort();
    const bool listening = fListen;
    int nPeers = 0;
    if (clientModel)
        nPeers = clientModel->getNumConnections();

    if (!clientModel) {
        peersLabel->setText("Starting this node…");
    } else if (nPeers <= 0 && !hasJoin) {
        peersLabel->setText(
            QString("You are the first node. Opening this wallet started it and it is listening on P2P port %1. "
                    "Other wallets do not find you through GitHub.")
                .arg(listenPort));
    } else if (nPeers <= 0 && hasJoin) {
        peersLabel->setText(
            QString("This node is running. Connecting to the seed in this package (port %1)…")
                .arg(listenPort));
    } else if (nPeers == 1) {
        peersLabel->setText("Connected to 1 peer");
    } else {
        peersLabel->setText(QString("Connected to %1 peers").arg(nPeers));
    }

    if (nodeStatusLabel) {
        QString listenLine;
        if (listening)
            listenLine = QString("Listen is on. Port %1.").arg(listenPort);
        else
            listenLine = QString("Listen is off. Other wallets cannot connect here.");
        if (nPeers <= 0 && !hasJoin) {
            nodeStatusLabel->setText(
                listenLine + "\n"
                "You are the first node. This running wallet is the seed.\n"
                "Provide my node IP (off by default) is the switch that shows the address others must add. "
                "Turn it on only when you want to copy host:" + QString::number(listenPort) +
                " into the public package as addnode=. It does not publish your IP by itself.");
        } else if (nPeers <= 0 && hasJoin) {
            nodeStatusLabel->setText(
                listenLine + "\n"
                "This package already has a seed (addnode). No terminal. "
                "Provide my node IP stays off unless you want to share this computer's address too.");
        } else {
            nodeStatusLabel->setText(
                listenLine + "\n"
                "Provide my node IP stays off unless you want to share this computer's address.");
        }
    }

    if (shareNodeChk && shareNodeChk->isChecked())
        fillNodeShareWidgets();

    UniValue l;
    if (l.read(rpc("getlotteryinfo").toStdString()) && l.isObject() && !l.exists("error")) {
        const bool elig = l["local_eligible"].isTrue();
        const bool xv = l["local_x_verified"].isTrue();
        const QString handle = QString::fromStdString(l["local_xaccount"].getValStr());
        const QString next = QString::fromStdString(l["next_draw_height"].getValStr().empty()
                                                       ? l["height"].getValStr()
                                                       : l["next_draw_height"].getValStr());
        QString extra;
        if (l["local_is_winner"].isTrue())
            extra = "\nThis slot: winner";
        QString handleDisp = handle.isEmpty() ? QString("(sign in first)") : handle;
        if (!handle.isEmpty() && !handleDisp.startsWith(QLatin1Char('@')))
            handleDisp = QLatin1Char('@') + handleDisp;
        lotteryLabel->setObjectName(elig ? "xlotteryok" : "xlotteryno");
        lotteryLabel->setText(QString(
            "Lottery — X Verified only (invite list cannot exclude you)\n"
            "Eligible        %1\n"
            "X Verified      %2\n"
            "Next block      %3\n"
            "Handle          %4\n"
            "Active nodes    %5%6")
            .arg(elig ? "yes" : "no")
            .arg(xv ? "yes" : "no")
            .arg(next)
            .arg(handleDisp)
            .arg(QString::fromStdString(l["active_nodes"].getValStr()))
            .arg(extra));
        lotteryLabel->style()->unpolish(lotteryLabel);
        lotteryLabel->style()->polish(lotteryLabel);
    } else {
        lotteryLabel->setObjectName("xlotteryno");
        lotteryLabel->setText("Lottery\nNode starting…");
        lotteryLabel->style()->unpolish(lotteryLabel);
        lotteryLabel->style()->polish(lotteryLabel);
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
        assetLabel->setText("My asset\nNo root yet. Sign in, then Claim my root asset. "
                            "Allowlist is an optional invite list — it does not make you X Verified.");
    else
        assetLabel->setText("My asset\nRoot: " + rootName + "\nCreate a sub or unique on the Assets tab.");

    claimBtn->setEnabled(signedIn && rootName.isEmpty());
    allowlistBtn->setEnabled(signedIn);
}

QString XHome::localListenEndpoint() const
{
    int port = GetListenPort();
    if (port <= 0)
        port = 38443;

    QString host;
    int bestScore = -1;
    UniValue n;
    if (n.read(rpc("getnetworkinfo").toStdString()) && n.isObject()) {
        const UniValue& addrs = n["localaddresses"];
        if (addrs.isArray()) {
            for (size_t i = 0; i < addrs.size(); ++i) {
                const UniValue& rec = addrs[i];
                if (!rec.isObject())
                    continue;
                const QString addr = QString::fromStdString(rec["address"].getValStr());
                const int score = rec["score"].isNum() ? rec["score"].get_int() : 0;
                if (rec["port"].isNum())
                    port = rec["port"].get_int();
                if (addr.isEmpty() || addr.startsWith("127.") || addr == "::1")
                    continue;
                if (score >= bestScore) {
                    bestScore = score;
                    host = addr;
                }
            }
        }
    }
    if (host.isEmpty()) {
        host = QHostInfo::localHostName();
        if (host.isEmpty())
            host = QString("localhost");
    }
    if (host.contains(QLatin1Char(':')) && !host.startsWith(QLatin1Char('[')))
        return QString("[%1]:%2").arg(host).arg(port);
    return QString("%1:%2").arg(host).arg(port);
}

void XHome::fillNodeShareWidgets()
{
    if (!nodeEndpointLabel || !nodeIpEdit)
        return;
    const QString detected = localListenEndpoint();
    nodeEndpointLabel->setText(QString(
        "This is the address others must use\n%1\n\n"
        "Put this line in the public Windows and Linux package xcoin.conf, "
        "then open the GitHub repo:\n\n"
        "addnode=%1\n\n"
        "If that host is only a LAN name, type the public or VPN address instead. "
        "This screen does not invent an IP.")
        .arg(detected));
    if (nodeIpEdit->text().trimmed().isEmpty()) {
        QSettings settings;
        const QString saved = settings.value("xhomeShareNodeIpText").toString().trimmed();
        nodeIpEdit->setText(saved.isEmpty() ? detected : saved);
    }
}

void XHome::applyNodeShareVisibility(bool on)
{
    if (nodeSharePanel)
        nodeSharePanel->setVisible(on);
    if (!on) {
        if (nodeEndpointLabel)
            nodeEndpointLabel->clear();
        if (nodeIpEdit)
            nodeIpEdit->clear();
    } else {
        fillNodeShareWidgets();
    }
}

void XHome::onShareNodeToggled(bool on)
{
    QSettings settings;
    settings.setValue("xhomeShareNodeIp", on);
    if (!on && nodeIpEdit) {
        const QString typed = nodeIpEdit->text().trimmed();
        if (!typed.isEmpty())
            settings.setValue("xhomeShareNodeIpText", typed);
    }
    applyNodeShareVisibility(on);
}

void XHome::onCopyNodeAddress()
{
    QString text = nodeIpEdit ? nodeIpEdit->text().trimmed() : QString();
    if (text.isEmpty())
        text = localListenEndpoint();
    if (text.isEmpty()) {
        QMessageBox::information(this, "X-Coin",
            "Type the address you want to give out, or wait for the node to bind.");
        return;
    }
    if (!text.startsWith(QLatin1String("addnode="), Qt::CaseInsensitive))
        text = QString("addnode=%1").arg(text);
    QSettings settings;
    settings.setValue("xhomeShareNodeIpText", nodeIpEdit ? nodeIpEdit->text().trimmed() : QString());
    QApplication::clipboard()->setText(text);
    statusLabel->setText("Copied " + text + ". Put it in the public package xcoin.conf. This did not publish your IP.");
}

void XHome::onSignIn()
{
    if (XOAuth::cooldownRemainingMs() > 0) {
        statusLabel->setText("Sign in with X is cooling down (~1 hour). This wallet does not keep calling X.");
        applyAuthButtons(false);
        return;
    }
    statusLabel->setText("Opening X in your browser…");
    oauth->startLogin();
}

void XHome::onReLink()
{
    if (QMessageBox::question(this, "Re-link",
            "Re-link this wallet with X? This uses X API credits.\n"
            "Linked / Verified stay as the last successful sign-in until this finishes.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (XOAuth::cooldownRemainingMs() > 0) {
        QMessageBox::information(this, "X-Coin",
            "Wait about an hour between Sign in with X attempts. Failed or cancelled attempts count.");
        applyAuthButtons(true);
        return;
    }
    statusLabel->setText("Opening X in your browser…");
    oauth->startLogin();
}

void XHome::onMockSignIn()
{
    bool ok = false;
    QString handle = QInputDialog::getText(this, "Regtest simulate",
        "Mock GET /2/users/me (regtest only). Examples: NFTRVN  NFTRVN:verified  ghost:unverified. "
        "Compact handle with no flag defaults to X Verified (blue check). Use :unverified for zero lottery chance. "
        "This writes a session proof; it is not a live X login.",
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
        QMessageBox::warning(this, "X-Coin", "Sign in with X first. The invite list uses the session username, not a typed field. Allowlisting is not X Verified.");
        return;
    }
    showRpcOutcome(rpc("addxverified", QStringList() << QString::fromStdString(s["username"].getValStr())),
                   "Invite list updated");
    refresh();
}

void XHome::onClaim()
{
    showRpcOutcome(rpc("linkxaccount"), "Root asset claimed");
    refresh();
}

void XHome::onIssueSub()
{
    Q_EMIT gotoCreateSub(subNameEdit->text().trimmed());
}

void XHome::onIssueUnique()
{
    Q_EMIT gotoCreateUnique(uniqueNameEdit->text().trimmed());
}

void XHome::onOAuthSuccess(const QString& username, const QString& userId)
{
    Q_UNUSED(userId);
    statusLabel->setText(QString("This wallet is now linked to @%1. Sign-in stays on this computer.")
        .arg(username));
    refresh();
}

void XHome::onOAuthFailed(const QString& error)
{
    statusLabel->setObjectName("xstatuserr");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
    statusLabel->setText(error);
    refresh();
    QMessageBox::warning(this, "Sign in with X", error);
}

void XHome::onOAuthStatus(const QString& message)
{
    statusLabel->setText(message);
}
