// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xnodepage.h"

#include "chainparams.h"
#include "clientmodel.h"
#include "net.h"
#include "util.h"
#include "walletview.h"
#include "xtheme.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QHostInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

#include <univalue.h>

XNodePage::XNodePage(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , clientModel(0)
    , statusCard(0)
    , endpointLabel(0)
    , hintLabel(0)
    , nodeIpEdit(0)
    , shareChk(0)
    , sharePanel(0)
{
    applyTheme();

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* inner = new QWidget;
    QVBoxLayout* root = new QVBoxLayout(inner);
    root->setContentsMargins(40, 36, 40, 36);
    root->setSpacing(16);

    QLabel* title = new QLabel("MY NODE");
    title->setObjectName("xsection");
    root->addWidget(title);

    QLabel* tag = new QLabel("This wallet is already a node. Other people’s addresses stay hidden.");
    tag->setObjectName("xhint");
    tag->setWordWrap(true);
    root->addWidget(tag);

    statusCard = new QLabel;
    statusCard->setObjectName("xcard");
    statusCard->setWordWrap(true);
    root->addWidget(statusCard);

    shareChk = new QCheckBox("Provide my node IP");
    shareChk->setObjectName("xcheck");
    shareChk->setCursor(Qt::PointingHandCursor);
    shareChk->setToolTip("Off by default. Turn on to see the host:38443 address others must put in addnode=.");
    root->addWidget(shareChk);

    sharePanel = new QWidget;
    QVBoxLayout* shareLay = new QVBoxLayout(sharePanel);
    shareLay->setContentsMargins(0, 0, 0, 0);
    shareLay->setSpacing(10);

    hintLabel = new QLabel(
        "This is the address others must use. They do not find you through GitHub. "
        "Copy the addnode line into a package xcoin.conf only when you mean to share it.");
    hintLabel->setObjectName("xhint");
    hintLabel->setWordWrap(true);
    shareLay->addWidget(hintLabel);

    endpointLabel = new QLabel;
    endpointLabel->setObjectName("xcard");
    endpointLabel->setWordWrap(true);
    endpointLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    shareLay->addWidget(endpointLabel);

    QHBoxLayout* giveRow = new QHBoxLayout;
    nodeIpEdit = new QLineEdit;
    nodeIpEdit->setPlaceholderText("host:38443 — type a different address if the detected one is wrong");
    QPushButton* copyBtn = new QPushButton("Copy addnode line");
    copyBtn->setObjectName("xghost");
    copyBtn->setCursor(Qt::PointingHandCursor);
    giveRow->addWidget(nodeIpEdit, 1);
    giveRow->addWidget(copyBtn);
    shareLay->addLayout(giveRow);
    root->addWidget(sharePanel);
    root->addStretch(1);

    scroll->setWidget(inner);
    QVBoxLayout* wrap = new QVBoxLayout(this);
    wrap->setContentsMargins(0, 0, 0, 0);
    wrap->addWidget(scroll);

    QSettings settings;
    shareChk->setChecked(settings.value("xhomeShareNodeIp", false).toBool());
    applyShareVisibility(shareChk->isChecked());

    connect(shareChk, SIGNAL(toggled(bool)), this, SLOT(onShareToggled(bool)));
    connect(copyBtn, SIGNAL(clicked()), this, SLOT(onCopy()));

    QTimer* t = new QTimer(this);
    connect(t, SIGNAL(timeout()), this, SLOT(refresh()));
    t->start(4000);
    refresh();
}

void XNodePage::applyTheme()
{
    ApplyXPageTheme(this);
}

void XNodePage::setClientModel(ClientModel* model)
{
    if (clientModel)
        disconnect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(refresh()));
    clientModel = model;
    if (clientModel)
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(refresh()));
    refresh();
}

QString XNodePage::rpc(const QString& method, const QStringList& args) const
{
    if (!walletView) return QString();
    return walletView->callRpc(method, args);
}

void XNodePage::refresh()
{
    const bool hasJoin = !gArgs.GetArgs("-addnode").empty()
        || !gArgs.GetArgs("-seednode").empty()
        || (!gArgs.GetArgs("-connect").empty() && gArgs.GetArg("-connect", "0") != "0");
    int listenPort = GetListenPort();
    if (listenPort <= 0)
        listenPort = GetParams().GetDefaultPort();
    const bool listening = fListen;
    int nPeers = clientModel ? clientModel->getNumConnections() : 0;

    QString listenLine;
    if (listening)
        listenLine = QString("Listen is on. Port %1.").arg(listenPort);
    else
        listenLine = QString("Listen is off. Other wallets cannot connect here.");

    if (!clientModel) {
        statusCard->setText("Starting this node…");
    } else if (nPeers <= 0 && !hasJoin) {
        statusCard->setText(listenLine + "\nYou are the first node. This running wallet is the seed.");
    } else if (nPeers <= 0 && hasJoin) {
        statusCard->setText(listenLine + "\nConnecting to the seed in this package…");
    } else if (nPeers == 1) {
        statusCard->setText(listenLine + "\nConnected to 1 peer.");
    } else {
        statusCard->setText(listenLine + QString("\nConnected to %1 peers.").arg(nPeers));
    }

    if (shareChk && shareChk->isChecked())
        fillShareWidgets();
}

QString XNodePage::localListenEndpoint() const
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

void XNodePage::fillShareWidgets()
{
    if (!endpointLabel || !nodeIpEdit)
        return;
    const QString detected = localListenEndpoint();
    endpointLabel->setText(QString("Detected address\n%1\n\naddnode=%1").arg(detected));
    if (nodeIpEdit->text().trimmed().isEmpty()) {
        QSettings settings;
        const QString saved = settings.value("xhomeShareNodeIpText").toString().trimmed();
        nodeIpEdit->setText(saved.isEmpty() ? detected : saved);
    }
}

void XNodePage::applyShareVisibility(bool on)
{
    if (sharePanel)
        sharePanel->setVisible(on);
    if (!on) {
        if (endpointLabel)
            endpointLabel->clear();
        if (nodeIpEdit)
            nodeIpEdit->clear();
    } else {
        fillShareWidgets();
    }
}

void XNodePage::onShareToggled(bool on)
{
    QSettings settings;
    settings.setValue("xhomeShareNodeIp", on);
    if (!on && nodeIpEdit) {
        const QString typed = nodeIpEdit->text().trimmed();
        if (!typed.isEmpty())
            settings.setValue("xhomeShareNodeIpText", typed);
    }
    applyShareVisibility(on);
}

void XNodePage::onCopy()
{
    QString text = nodeIpEdit ? nodeIpEdit->text().trimmed() : QString();
    if (text.isEmpty())
        text = localListenEndpoint();
    if (text.isEmpty()) {
        QMessageBox::information(this, "X-Coin", "Type the address you want to give out, or wait for the node to bind.");
        return;
    }
    if (!text.startsWith(QLatin1String("addnode="), Qt::CaseInsensitive))
        text = QString("addnode=%1").arg(text);
    QSettings settings;
    settings.setValue("xhomeShareNodeIpText", nodeIpEdit ? nodeIpEdit->text().trimmed() : QString());
    QApplication::clipboard()->setText(text);
    statusCard->setText("Copied " + text + ". This did not publish your IP.");
}
