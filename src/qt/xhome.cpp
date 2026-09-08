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
    "QLabel#xhero { color: #ffffff; font-size: 42px; font-weight: 800; letter-spacing: 10px; }"
    "QLabel#xtag { color: #a1a1aa; font-size: 13px; }"
    "QLabel#xsection { color: #ffffff; font-size: 12px; font-weight: 800; letter-spacing: 3px; margin-top: 8px; }"
    "QLabel#xcard { background: #0a0a0a; border: 1px solid #27272a; border-radius: 0px; padding: 14px; color: #e4e4e7; font-size: 13px; }"
    "QLabel#xlinked { background: #0a0a0a; border: 1px solid #ffffff; border-radius: 0px; padding: 16px; color: #ffffff; font-size: 13px; }"
    "QLabel#xunlinked { background: #0a0a0a; border: 1px solid #3f3f46; border-radius: 0px; padding: 16px; color: #a1a1aa; font-size: 13px; }"
    "QLabel#xhint { color: #71717a; font-size: 12px; }"
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
    , copyNodeBtn(0)
    , shareNodeChk(0)
    , nodeSharePanel(0)
    , nodeEndpointLabel(0)
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

    QLabel* tag = new QLabel("Private test chain · this wallet · your X account");
    tag->setObjectName("xtag");
    tag->setAlignment(Qt::AlignHCenter);
    tag->setWordWrap(true);
    root->addWidget(tag);

    // Count only. Never list other people's addresses here — every xcoin-qt is already a node.
    // A user's node IP is their choice to provide. Off by default; never show automatically.
    // Joining a mesh still uses addnode= in xcoin.conf (operator docs).
    peersLabel = new QLabel("Connecting…");
    peersLabel->setObjectName("xtag");
    peersLabel->setAlignment(Qt::AlignHCenter);
    peersLabel->setWordWrap(true);
    root->addWidget(peersLabel);

    balanceLabel = new QLabel;
    balanceLabel->setObjectName("xcard");
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
        "This wallet + this X session = you. Another user cannot send from inside your wallet.\n"
        "12-word BIP39 seed — still required. It creates and restores the keys in this wallet.dat.\n"
        "Sign in with X — session proof in this datadir (xsession.json + xsession.key). It gates send, receive, and own on this node. It does not import someone else's keys.\n"
        "Spend only keys in this wallet.dat. Signing in as @alice on Bob's empty wallet cannot spend Alice's UTXOs. A typed handle cannot steal this.");
    root->addWidget(keysLabel);

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
        "Operator: paste your X app Client ID. The loopback callback URL is in "
        "docs/XSIGNIN.md. Nothing is hardcoded. Login never succeeds without a "
        "real token (or a -regtest mock of GET /2/users/me).");
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
        "Receive, Send, Activity, and Transfer assets are all in this window — no terminal. "
        "Send and transfer spend only keys in this wallet.dat.");
    moneyHint->setObjectName("xhint");
    moneyHint->setWordWrap(true);
    root->addWidget(moneyHint);

    QLabel* nodeHead = new QLabel("MY NODE");
    nodeHead->setObjectName("xsection");
    root->addWidget(nodeHead);

    shareNodeChk = new QCheckBox("Provide my node IP");
    shareNodeChk->setObjectName("xcheck");
    shareNodeChk->setCursor(Qt::PointingHandCursor);
    shareNodeChk->setToolTip("Optional. Off by default. Your node IP is yours to provide — never shown automatically, and other people's IPs are never listed.");
    root->addWidget(shareNodeChk);

    nodeSharePanel = new QWidget;
    QVBoxLayout* nodeLay = new QVBoxLayout(nodeSharePanel);
    nodeLay->setContentsMargins(0, 0, 0, 0);
    nodeLay->setSpacing(10);

    QLabel* nodeHint = new QLabel(
        "Optional. This is your listen address — give it to a friend or seed operator if you want. "
        "Nobody is required to share. Other people's IPs are never listed here. "
        "Joining a mesh still uses addnode= in xcoin.conf.");
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
    nodeIpEdit->setPlaceholderText("host:38443 — type the address you want to give out");
    copyNodeBtn = new QPushButton("Copy my node address");
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
    connect(mockBtn, SIGNAL(clicked()), this, SLOT(onMockSignIn()));
    connect(saveId, SIGNAL(clicked()), this, SLOT(onSaveClientId()));
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

void XHome::refresh()
{
    UniValue s;
    s.read(rpc("getxsession").toStdString());
    const bool signedIn = s.isObject() && (s["signed_in"].isTrue() || s["linked"].isTrue());
    if (signedIn) {
        QString uid = QString::fromStdString(s["user_id"].getValStr());
        if (uid.isEmpty())
            uid = QString::fromStdString(s["id"].getValStr());
        const QString handle = QString::fromStdString(s["username"].getValStr());
        const bool xVerified = s["x_verified"].isTrue() || s["verified"].isTrue();
        QString proof = QString::fromStdString(s["session_file"].getValStr());
        QString secret = QString::fromStdString(s["secret_file"].getValStr());
        if (proof.isEmpty())
            proof = "xsession.json";
        if (secret.isEmpty())
            secret = "xsession.key";
        const QString vtype = QString::fromStdString(s["verified_type"].getValStr());
        QString verifiedLine;
        if (xVerified) {
            verifiedLine = QString(
                "X Verified: yes (%1). This running wallet cannot be excluded from the lottery.\n")
                .arg(vtype.isEmpty() ? QString("blue check / X Premium") : vtype + " check");
        } else {
            verifiedLine = QString(
                "X Verified: no. X has not marked @%1 verified (no blue / business / government check).\n"
                "Lottery is closed. “Allowlist my handle” is an operator invite list — it does not make you X Verified.\n")
                .arg(handle);
        }
        sessionLabel->setObjectName("xlinked");
        sessionLabel->setText(QString(
            "THIS WALLET IS YOURS\n"
            "Linked to @%1\n"
            "X user id %2\n"
            "%3\n"
            "The session proof in this datadir is what links this wallet to that X account:\n"
            "%4\n"
            "%5\n\n"
            "Send, receive, and own require that proof. Another signed-in identity cannot spend this wallet.dat.\n"
            "You cannot open someone else's coins or assets just by typing their @handle.\n"
            "The 12-word seed still controls the keys. Sign in with X is the identity proof on this node.")
            .arg(handle)
            .arg(uid)
            .arg(verifiedLine.trimmed())
            .arg(proof)
            .arg(secret));
        signInBtn->setText("Wallet linked to @" + handle);
    } else {
        sessionLabel->setObjectName("xunlinked");
        sessionLabel->setText(
            "THIS WALLET IS NOT LINKED YET\n"
            "Sign in with X to bind this node to your X account.\n\n"
            "Send, receive, and own require a session proof (xsession.json + xsession.key) "
            "written only after Sign in with X. A typed handle cannot steal this.\n"
            "This wallet + this X session = you. Another user cannot send from inside your wallet.\n"
            "The 12-word seed still controls the keys. Sign in with X is the identity gate, not a replacement for the seed.\n"
            "Only X Verified handles (X's blue check / X Premium, plus business and government org checks) enter the lottery. "
            "That is what GET /2/users/me reports — not the operator invite list. A verified running wallet cannot be excluded. "
            "An account that is not blue-check verified has zero chance. Every signed-in user can send and receive.");
        signInBtn->setText("Sign in with X");
    }
    sessionLabel->style()->unpolish(sessionLabel);
    sessionLabel->style()->polish(sessionLabel);

    if (walletModel) {
        const int unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : 0;
        balanceLabel->setText("Balance\n" + RavenUnits::formatWithUnit(unit, walletModel->getBalance()));
    } else {
        balanceLabel->setText("Balance\n(open or create a wallet from File)");
    }

    if (clientModel) {
        const int n = clientModel->getNumConnections();
        if (n <= 0)
            peersLabel->setText("Connecting…");
        else if (n == 1)
            peersLabel->setText("Connected to 1 peer");
        else
            peersLabel->setText(QString("Connected to %1 peers").arg(n));
    } else {
        peersLabel->setText("Connecting…");
    }

    if (shareNodeChk && shareNodeChk->isChecked())
        fillNodeShareWidgets();

    UniValue l;
    if (l.read(rpc("getlotteryinfo").toStdString()) && l.isObject()) {
        const bool elig = l["local_eligible"].isTrue();
        const QString handle = QString::fromStdString(l["local_xaccount"].getValStr());
        lotteryLabel->setText(QString("Lottery (X Verified — blue check; invite list cannot exclude you)\nEligible: %1\nNext draw: block %2 (one block per minute slot)\nYour handle: %3\nActive nodes: %4")
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
        assetLabel->setText("My asset\nNo root yet. After you sign in, Claim my root asset. "
                            "Allowlist my handle is an optional operator invite list for private test — it does not make you X Verified. "
                            "The node uses the signed-in username only — a typed foreign handle is rejected.");
    else
        assetLabel->setText("My asset\nRoot: " + rootName + "\nIssue a sub or unique below — no CLI.");

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
        "This machine's listen endpoint\n%1\n\n"
        "Copy this, or type the IP you want to give out (public, VPN, or LAN).")
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
    QSettings settings;
    settings.setValue("xhomeShareNodeIpText", text);
    QApplication::clipboard()->setText(text);
    statusLabel->setText("Copied your node address. Give it only if you choose to.");
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
    statusLabel->setText(QString("This wallet is now linked to @%1 (X user id %2). Session proof written in this datadir.")
        .arg(username).arg(userId));
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
