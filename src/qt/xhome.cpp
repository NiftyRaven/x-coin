// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xhome.h"

#include "askpassphrasedialog.h"
#include "assets/xaccount.h"
#include "chainparams.h"
#include "walletview.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "optionsmodel.h"
#include "ravenunits.h"
#include "xoauth.h"
#include "xrelease.h"
#include "xreleasedialog.h"
#include "xsession.h"
#include "xtheme.h"
#include "guiutil.h"
#include "net.h"
#include "util.h"
#include "utiltime.h"

#include <QFrame>
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QObject>

#include <univalue.h>

namespace {

bool pickUnlockSeconds(QWidget* parent, int64_t* outSeconds)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("How long to stay unlocked"));
    dlg.setMinimumWidth(420);
    QVBoxLayout* lay = new QVBoxLayout(&dlg);
    QLabel* hint = new QLabel(QStringLiteral(
        "The wallet stays unlocked for this long, then locks itself. "
        "Unlocking lets this node heartbeat for the lottery. Locking stops that."));
    hint->setWordWrap(true);
    lay->addWidget(hint);

    QRadioButton* m5 = new QRadioButton(QStringLiteral("5 minutes"));
    QRadioButton* m15 = new QRadioButton(QStringLiteral("15 minutes"));
    QRadioButton* h1 = new QRadioButton(QStringLiteral("1 hour"));
    QRadioButton* h8 = new QRadioButton(QStringLiteral("8 hours"));
    QRadioButton* untilLock = new QRadioButton(QStringLiteral("Until I lock this wallet"));
    QRadioButton* custom = new QRadioButton(QStringLiteral("Custom minutes"));
    m15->setChecked(true);

    QSpinBox* mins = new QSpinBox;
    mins->setRange(1, 10080);
    mins->setValue(30);
    mins->setSuffix(QStringLiteral(" min"));
    mins->setEnabled(false);

    QButtonGroup* g = new QButtonGroup(&dlg);
    g->addButton(m5);
    g->addButton(m15);
    g->addButton(h1);
    g->addButton(h8);
    g->addButton(untilLock);
    g->addButton(custom);
    lay->addWidget(m5);
    lay->addWidget(m15);
    lay->addWidget(h1);
    lay->addWidget(h8);
    lay->addWidget(untilLock);
    QHBoxLayout* customRow = new QHBoxLayout;
    customRow->addWidget(custom);
    customRow->addWidget(mins);
    customRow->addStretch(1);
    lay->addLayout(customRow);

    QObject::connect(custom, SIGNAL(toggled(bool)), mins, SLOT(setEnabled(bool)));

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(box, SIGNAL(accepted()), &dlg, SLOT(accept()));
    QObject::connect(box, SIGNAL(rejected()), &dlg, SLOT(reject()));
    lay->addWidget(box);

    if (dlg.exec() != QDialog::Accepted)
        return false;
    if (m5->isChecked())
        *outSeconds = 5 * 60;
    else if (m15->isChecked())
        *outSeconds = 15 * 60;
    else if (h1->isChecked())
        *outSeconds = 60 * 60;
    else if (h8->isChecked())
        *outSeconds = 8 * 60 * 60;
    else if (untilLock->isChecked())
        *outSeconds = 0;
    else
        *outSeconds = (int64_t)mins->value() * 60;
    return true;
}

} // namespace

XHome::XHome(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , clientModel(0)
    , walletModel(0)
    , oauth(new XOAuth(this))
    , signInBtn(0)
    , reLinkBtn(0)
    , mockBtn(0)
    , claimBtn(0)
    , allowlistBtn(0)
    , unlockBtn(0)
    , lockBtn(0)
    , releasePanel(0)
    , releaseLabel(0)
    , releaseNotesBtn(0)
    , releaseLaterBtn(0)
{
    applyTheme();

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: #000000; border: none; }");

    QWidget* inner = new QWidget;
    inner->setObjectName("xhomeInner");
    QVBoxLayout* root = new QVBoxLayout(inner);
    root->setContentsMargins(40, 28, 40, 36);
    root->setSpacing(16);

    heroTitle = new QLabel("X");
    heroTitle->setObjectName("xhero");
    heroTitle->setAlignment(Qt::AlignHCenter);
    root->addWidget(heroTitle);

    QLabel* word = new QLabel("X-COIN");
    word->setObjectName("xsection");
    word->setAlignment(Qt::AlignHCenter);
    root->addWidget(word);

    QLabel* tag = new QLabel("Your name. Your wallet.");
    tag->setObjectName("xtag");
    tag->setAlignment(Qt::AlignHCenter);
    tag->setWordWrap(true);
    root->addWidget(tag);

    versionLabel = new QLabel;
    versionLabel->setObjectName("xversion");
    versionLabel->setAlignment(Qt::AlignHCenter);
    versionLabel->setText(QString("Wallet %1 %2")
        .arg(QString::fromStdString(xrelease::RunningVersionString()))
        .arg(QString::fromStdString(xrelease::WalletEdition())));
    root->addWidget(versionLabel);

    releasePanel = new QWidget;
    QVBoxLayout* relLay = new QVBoxLayout(releasePanel);
    relLay->setContentsMargins(0, 0, 0, 0);
    relLay->setSpacing(10);
    releaseLabel = new QLabel;
    releaseLabel->setObjectName("xrelease");
    releaseLabel->setWordWrap(true);
    relLay->addWidget(releaseLabel);
    QHBoxLayout* relRow = new QHBoxLayout;
    releaseNotesBtn = new QPushButton("What's new");
    releaseNotesBtn->setObjectName("xprimary");
    releaseNotesBtn->setCursor(Qt::PointingHandCursor);
    releaseLaterBtn = new QPushButton("Later");
    releaseLaterBtn->setObjectName("xghost");
    releaseLaterBtn->setCursor(Qt::PointingHandCursor);
    relRow->addWidget(releaseNotesBtn);
    relRow->addWidget(releaseLaterBtn);
    relRow->addStretch(1);
    relLay->addLayout(relRow);
    releasePanel->setVisible(false);
    root->addWidget(releasePanel);

    peersLabel = new QLabel("Starting this node…");
    peersLabel->setObjectName("xtag");
    peersLabel->setAlignment(Qt::AlignHCenter);
    peersLabel->setWordWrap(true);
    root->addWidget(peersLabel);

    balanceLabel = new QLabel;
    balanceLabel->setObjectName("xbalance");
    balanceLabel->setWordWrap(true);
    root->addWidget(balanceLabel);

    lockLabel = new QLabel;
    lockLabel->setObjectName("xcard");
    lockLabel->setWordWrap(true);
    lockLabel->setVisible(false);
    root->addWidget(lockLabel);

    QHBoxLayout* lockRow = new QHBoxLayout;
    unlockBtn = new QPushButton("Unlock wallet");
    unlockBtn->setObjectName("xprimary");
    unlockBtn->setMinimumHeight(48);
    unlockBtn->setCursor(Qt::PointingHandCursor);
    unlockBtn->setVisible(false);
    lockBtn = new QPushButton("Lock wallet");
    lockBtn->setObjectName("xghost");
    lockBtn->setMinimumHeight(48);
    lockBtn->setCursor(Qt::PointingHandCursor);
    lockBtn->setVisible(false);
    lockRow->addWidget(unlockBtn);
    lockRow->addWidget(lockBtn);
    lockRow->addStretch(1);
    root->addLayout(lockRow);

    sessionLabel = new QLabel;
    sessionLabel->setObjectName("xunlinked");
    sessionLabel->setWordWrap(true);
    root->addWidget(sessionLabel);

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

    QLabel* credHint = new QLabel("Sign in with X opens your browser. Come back here when it finishes.");
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
    claimBtn = new QPushButton("Claim my name");
    claimBtn->setObjectName("xprimary");
    claimBtn->setMinimumHeight(44);
    claimBtn->setCursor(Qt::PointingHandCursor);
    claimRow->addWidget(allowlistBtn);
    claimRow->addWidget(claimBtn);
    root->addLayout(claimRow);

    QHBoxLayout* money = new QHBoxLayout;
    QPushButton* recv = new QPushButton("Receive");
    QPushButton* send = new QPushButton("Send");
    QPushButton* activity = new QPushButton("Activity");
    recv->setObjectName("xprimary");
    send->setObjectName("xprimary");
    activity->setObjectName("xghost");
    recv->setMinimumHeight(44);
    send->setMinimumHeight(44);
    activity->setMinimumHeight(44);
    recv->setCursor(Qt::PointingHandCursor);
    send->setCursor(Qt::PointingHandCursor);
    activity->setCursor(Qt::PointingHandCursor);
    money->addWidget(recv);
    money->addWidget(send);
    money->addWidget(activity);
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
    connect(reLinkBtn, SIGNAL(clicked()), this, SLOT(onReLink()));
    connect(mockBtn, SIGNAL(clicked()), this, SLOT(onMockSignIn()));
    connect(allowlistBtn, SIGNAL(clicked()), this, SLOT(onAllowlistMe()));
    connect(claimBtn, SIGNAL(clicked()), this, SLOT(onClaim()));
    connect(recv, SIGNAL(clicked()), this, SIGNAL(gotoReceive()));
    connect(send, SIGNAL(clicked()), this, SIGNAL(gotoSend()));
    connect(activity, SIGNAL(clicked()), this, SIGNAL(gotoActivity()));
    connect(oauth, SIGNAL(signedIn(QString,QString)), this, SLOT(onOAuthSuccess(QString,QString)));
    connect(oauth, SIGNAL(failed(QString)), this, SLOT(onOAuthFailed(QString)));
    connect(oauth, SIGNAL(status(QString)), this, SLOT(onOAuthStatus(QString)));
    connect(releaseNotesBtn, SIGNAL(clicked()), this, SLOT(onReleaseNotes()));
    connect(releaseLaterBtn, SIGNAL(clicked()), this, SLOT(onReleaseLater()));
    connect(unlockBtn, SIGNAL(clicked()), this, SLOT(onUnlockWallet()));
    connect(lockBtn, SIGNAL(clicked()), this, SLOT(onLockWallet()));

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
    ApplyXPageTheme(this);
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
    if (walletModel)
        disconnect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(refresh()));
    walletModel = model;
    if (walletModel)
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(refresh()));
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
    xsession::Session sess;
    std::string serr;
    const bool signedIn = xsession::LoadSession(sess, serr);
    if (signedIn) {
        const QString handle = QString::fromStdString(sess.username);
        const bool xVerified = sess.IsXVerified();
        const QString vtype = QString::fromStdString(sess.verifiedType);
        QString verifiedLine;
        if (xVerified) {
            verifiedLine = QString("X Verified: yes (%1)")
                .arg(vtype.isEmpty() ? QString("blue check") : vtype + " check");
        } else {
            verifiedLine = QString("X Verified: no. Lottery chance is zero.");
        }
        sessionLabel->setObjectName("xlinked");
        sessionLabel->setText(QString("@%1\n%2").arg(handle).arg(verifiedLine));
    } else {
        sessionLabel->setObjectName("xunlinked");
        sessionLabel->setText(
            "Not linked yet. Sign in with X to put your name on this wallet.\n"
            "You can still receive and send. Sign in to claim your free name.");
    }
    applyAuthButtons(signedIn);
    sessionLabel->style()->unpolish(sessionLabel);
    sessionLabel->style()->polish(sessionLabel);

    if (walletModel) {
        const int unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : 0;
        balanceLabel->setText(RavenUnits::formatWithUnit(unit, walletModel->getBalance()));
    } else {
        balanceLabel->setText("Open or create a wallet from File");
    }

    const WalletModel::EncryptionStatus enc = walletModel
        ? walletModel->getEncryptionStatus() : WalletModel::Unencrypted;
    const bool showUnlock = (enc == WalletModel::Locked);
    const bool showLock = (enc == WalletModel::Unlocked);
    if (lockLabel) {
        lockLabel->setVisible(showUnlock || showLock);
        if (showUnlock) {
            lockLabel->setText("Wallet locked. Unlock to send and to heartbeat for the lottery.");
        } else if (showLock) {
            const int64_t until = walletModel->getUnlockUntil();
            if (until > 0) {
                const int64_t left = until - GetTime();
                if (left > 0) {
                    const int64_t minsLeft = (left + 59) / 60;
                    lockLabel->setText(QString("Wallet unlocked for about %1 more minute(s). Locking stops lottery heartbeat.")
                        .arg(minsLeft));
                } else {
                    lockLabel->setText("Wallet unlocked. Locking stops lottery heartbeat.");
                }
            } else {
                lockLabel->setText("Wallet unlocked until you lock it. Locking stops lottery heartbeat.");
            }
        }
    }
    if (unlockBtn)
        unlockBtn->setVisible(showUnlock);
    if (lockBtn)
        lockBtn->setVisible(showLock);

    const bool hasJoin = !gArgs.GetArgs("-addnode").empty()
        || !gArgs.GetArgs("-seednode").empty()
        || (!gArgs.GetArgs("-connect").empty() && gArgs.GetArg("-connect", "0") != "0");
    int nSeed = 0, nPublic = 0;
    if (clientModel)
        clientModel->getPeerKinds(nSeed, nPublic);
    const int nPeers = nSeed + nPublic;
    if (!clientModel)
        peersLabel->setText("Starting this node…");
    else if (nPeers <= 0 && !hasJoin)
        peersLabel->setText("You are the first node.");
    else if (nPeers <= 0)
        peersLabel->setText("Connecting…");
    else
        peersLabel->setText(GUIUtil::FormatXCoinPeerLine(nSeed, nPublic));

    UniValue l;
    if (l.read(rpc("getlotteryinfo").toStdString()) && l.isObject() && !l.exists("error")) {
        const bool elig = l["local_eligible"].isTrue();
        const bool xv = l["local_x_verified"].isTrue();
        QString handle = QString::fromStdString(l["local_xaccount"].getValStr());
        if (!handle.isEmpty() && !handle.startsWith(QLatin1Char('@')))
            handle = QLatin1Char('@') + handle;
        const QString next = QString::fromStdString(l["next_draw_height"].getValStr().empty()
                                                       ? l["height"].getValStr()
                                                       : l["next_draw_height"].getValStr());
        QString extra;
        if (l["local_is_winner"].isTrue())
            extra = "  ·  this slot: winner";
        lotteryLabel->setObjectName(elig ? "xlotteryok" : "xlotteryno");
        if (elig)
            lotteryLabel->setText(QString("In this minute’s draw  ·  %1  ·  next block %2%3")
                .arg(handle).arg(next).arg(extra));
        else if (enc == WalletModel::Locked)
            lotteryLabel->setText("Not in this draw. Unlock the wallet to heartbeat.");
        else if (!xv)
            lotteryLabel->setText("Not in this draw. Sign in with an X Verified account to enter.");
        else
            lotteryLabel->setText(QString("Not in this draw yet  ·  %1  ·  next block %2")
                .arg(handle.isEmpty() ? QString("sign in") : handle).arg(next));
        lotteryLabel->style()->unpolish(lotteryLabel);
        lotteryLabel->style()->polish(lotteryLabel);
    } else {
        lotteryLabel->setObjectName("xlotteryno");
        lotteryLabel->setText("Lottery — node starting…");
        lotteryLabel->style()->unpolish(lotteryLabel);
        lotteryLabel->style()->polish(lotteryLabel);
    }

    QString rootName;
    if (signedIn) {
        std::string expect, err;
        if (MapXHandleToRootName(sess.username, expect, err)) {
            UniValue a;
            if (a.read(rpc("listmyassets").toStdString()) && a.isObject()) {
                if (a.exists(expect) || a.exists(expect + "!"))
                    rootName = QString::fromStdString(expect);
            }
        }
    }
    if (rootName.isEmpty())
        assetLabel->setText("No name claimed yet. Sign in, then Claim my name.");
    else
        assetLabel->setText("Name asset  ·  " + rootName);

    claimBtn->setEnabled(signedIn && rootName.isEmpty());
    allowlistBtn->setEnabled(signedIn);
    allowlistBtn->setVisible(signedIn && rootName.isEmpty());

    if (releasePanel && releaseLabel) {
        xrelease::Release newer;
        QSettings rel;
        const QString dismissed = rel.value("xrelease/dismissedTag").toString();
        const QString seen = rel.value("xrelease/seenCurrent").toString();
        const QString cur = QString::fromStdString(xrelease::RunningTag());
        const bool showNewer = xrelease::GetCachedNewer(newer)
            && QString::fromStdString(newer.tag) != dismissed;
        const bool showThis = !showNewer && seen != cur;
        releasePanel->setVisible(showNewer || showThis);
        if (showNewer) {
            const QString ver = QString::fromStdString(newer.tag);
            const QString title = QString::fromStdString(newer.name.empty() ? newer.tag : newer.name);
            releaseLabel->setText(QString("New wallet %1 is on GitHub Releases.\n%2").arg(ver).arg(title));
            if (releaseNotesBtn)
                releaseNotesBtn->setText("What's new");
            if (releaseLaterBtn)
                releaseLaterBtn->setText("Later");
        } else if (showThis) {
            releaseLabel->setText(QString("This wallet is %1.").arg(cur));
            if (releaseNotesBtn)
                releaseNotesBtn->setText("What's new");
            if (releaseLaterBtn)
                releaseLaterBtn->setText("OK");
        }
    }
}

void XHome::onSignIn()
{
    if (XOAuth::cooldownRemainingMs() > 0) {
        statusLabel->setText("Sign in with X is cooling down (~1 hour).");
        applyAuthButtons(xsession::HasValidSession());
        return;
    }
    statusLabel->setText("Opening X in your browser…");
    oauth->startLogin();
}

void XHome::onReLink()
{
    if (QMessageBox::question(this, "Re-link",
            "Re-link this wallet with X? This uses X API credits.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (XOAuth::cooldownRemainingMs() > 0) {
        QMessageBox::information(this, "X-Coin",
            "Wait about an hour between Sign in with X attempts.");
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
        "Mock GET /2/users/me (regtest only). Examples: NFTRVN  NFTRVN:verified  ghost:unverified.",
        QLineEdit::Normal, "NFTRVN", &ok);
    if (!ok || handle.trimmed().isEmpty()) return;
    QString err;
    if (!oauth->mockSignIn(handle.trimmed(), err)) {
        QMessageBox::warning(this, "X-Coin", err);
        return;
    }
    statusLabel->setText("Regtest session injected.");
    refresh();
}

void XHome::onAllowlistMe()
{
    xsession::Session sess;
    std::string serr;
    if (!xsession::LoadSession(sess, serr)) {
        QMessageBox::warning(this, "X-Coin", "Sign in with X first.");
        return;
    }
    showRpcOutcome(rpc("addxverified", QStringList() << QString::fromStdString(sess.username)),
                   "Invite list updated");
    refresh();
}

void XHome::onClaim()
{
    showRpcOutcome(rpc("linkxaccount"), "Name claimed");
    refresh();
}

void XHome::onOAuthSuccess(const QString& username, const QString& userId)
{
    Q_UNUSED(userId);
    statusLabel->setText(QString("This wallet is now linked to @%1.").arg(username));
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

void XHome::onReleaseNotes()
{
    xrelease::Release newer;
    QSettings rel;
    if (!xrelease::GetCachedNewer(newer)) {
        const xrelease::Release bundled = xrelease::BundledRelease();
        XReleaseDialog dlg(this,
                           QString::fromStdString(bundled.name),
                           QString::fromStdString(bundled.body),
                           QString::fromStdString(bundled.htmlUrl),
                           false);
        dlg.exec();
        rel.setValue("xrelease/seenCurrent", QString::fromStdString(xrelease::RunningTag()));
        if (releasePanel)
            releasePanel->setVisible(false);
        return;
    }
    XReleaseDialog dlg(this,
                       QString::fromStdString(newer.name.empty() ? newer.tag : newer.name),
                       QString::fromStdString(newer.body.empty() ? xrelease::BundledNotes() : newer.body),
                       QString::fromStdString(newer.htmlUrl),
                       true);
    dlg.exec();
    rel.setValue("xrelease/seenCurrent", QString::fromStdString(xrelease::RunningTag()));
    if (dlg.dismissed())
        onReleaseLater();
}

void XHome::onReleaseLater()
{
    QSettings rel;
    rel.setValue("xrelease/seenCurrent", QString::fromStdString(xrelease::RunningTag()));
    xrelease::Release newer;
    if (xrelease::GetCachedNewer(newer))
        rel.setValue("xrelease/dismissedTag", QString::fromStdString(newer.tag));
    if (releasePanel)
        releasePanel->setVisible(false);
}

void XHome::onUnlockWallet()
{
    if (!walletModel || walletModel->getEncryptionStatus() != WalletModel::Locked)
        return;
    int64_t seconds = 0;
    if (!pickUnlockSeconds(this, &seconds))
        return;
    AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
    dlg.setModel(walletModel);
    dlg.setUnlockTimeout(seconds);
    dlg.exec();
    refresh();
}

void XHome::onLockWallet()
{
    if (!walletModel || walletModel->getEncryptionStatus() != WalletModel::Unlocked)
        return;
    walletModel->setWalletLocked(true);
    refresh();
}
