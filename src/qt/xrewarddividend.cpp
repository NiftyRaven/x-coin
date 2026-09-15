// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xrewarddividend.h"

#include "walletview.h"
#include "xtheme.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

#include <univalue.h>

XRewardDividend::XRewardDividend(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , targetCombo(0)
    , payCombo(0)
    , amountEdit(0)
    , heightEdit(0)
    , excludeEdit(0)
    , logEdit(0)
    , statusLabel(0)
{
    applyTheme();

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* inner = new QWidget;
    QVBoxLayout* root = new QVBoxLayout(inner);
    root->setContentsMargins(40, 36, 40, 36);
    root->setSpacing(14);

    QLabel* title = new QLabel("ASSET DIVIDENDS");
    title->setObjectName("xsection");
    root->addWidget(title);

    QLabel* hint = new QLabel(
        "Pay holders of one asset (a root or sub — not a unique). "
        "First schedule a snapshot at a future block. After that block is 60 confirmations old, pay XFER or another asset.");
    hint->setObjectName("xhint");
    hint->setWordWrap(true);
    root->addWidget(hint);

    QLabel* who = new QLabel("WHO GETS PAID");
    who->setObjectName("xsection");
    root->addWidget(who);

    targetCombo = new QComboBox;
    targetCombo->setEditable(true);
    targetCombo->setInsertPolicy(QComboBox::NoInsert);
    root->addWidget(targetCombo);

    QPushButton* refreshBtn = new QPushButton("Refresh my assets");
    refreshBtn->setObjectName("xghost");
    refreshBtn->setCursor(Qt::PointingHandCursor);
    root->addWidget(refreshBtn);

    QHBoxLayout* heightRow = new QHBoxLayout;
    QLabel* hLbl = new QLabel("Snapshot height");
    hLbl->setObjectName("xhint");
    heightEdit = new QLineEdit;
    heightEdit->setPlaceholderText("leave blank for the next block");
    heightRow->addWidget(hLbl);
    heightRow->addWidget(heightEdit, 1);
    root->addLayout(heightRow);

    QPushButton* snapBtn = new QPushButton("Schedule snapshot");
    snapBtn->setObjectName("xprimary");
    snapBtn->setMinimumHeight(44);
    snapBtn->setCursor(Qt::PointingHandCursor);
    root->addWidget(snapBtn);

    QLabel* payHead = new QLabel("WHAT TO PAY");
    payHead->setObjectName("xsection");
    root->addWidget(payHead);

    payCombo = new QComboBox;
    payCombo->setEditable(true);
    payCombo->addItem("XFER");
    root->addWidget(payCombo);

    amountEdit = new QLineEdit;
    amountEdit->setPlaceholderText("Total amount to split among holders");
    root->addWidget(amountEdit);

    excludeEdit = new QLineEdit;
    excludeEdit->setPlaceholderText("Exclude addresses (optional, comma-separated)");
    root->addWidget(excludeEdit);

    QPushButton* payBtn = new QPushButton("Pay holders");
    payBtn->setObjectName("xprimary");
    payBtn->setMinimumHeight(48);
    payBtn->setCursor(Qt::PointingHandCursor);
    root->addWidget(payBtn);

    logEdit = new QPlainTextEdit;
    logEdit->setReadOnly(true);
    logEdit->setMinimumHeight(140);
    logEdit->setPlaceholderText("Scheduled snapshots and results appear here.");
    root->addWidget(logEdit);

    statusLabel = new QLabel;
    statusLabel->setObjectName("xhint");
    statusLabel->setWordWrap(true);
    root->addWidget(statusLabel);
    root->addStretch(1);

    scroll->setWidget(inner);
    QVBoxLayout* wrap = new QVBoxLayout(this);
    wrap->setContentsMargins(0, 0, 0, 0);
    wrap->addWidget(scroll);

    connect(refreshBtn, SIGNAL(clicked()), this, SLOT(onRefreshAssets()));
    connect(snapBtn, SIGNAL(clicked()), this, SLOT(onScheduleSnapshot()));
    connect(payBtn, SIGNAL(clicked()), this, SLOT(onPay()));

    refresh();
}

void XRewardDividend::applyTheme()
{
    ApplyXPageTheme(this);
}

QString XRewardDividend::rpc(const QString& method, const QStringList& args) const
{
    if (!walletView) return QString();
    return walletView->callRpc(method, args);
}

void XRewardDividend::showRpcOutcome(const QString& raw, const QString& okPrefix)
{
    bool ok = false;
    const QString text = WalletView::humanRpc(raw, &ok);
    statusLabel->setObjectName(ok ? "xhint" : "xstatuserr");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
    if (ok) {
        const QString line = okPrefix.isEmpty() ? text : (text.isEmpty() ? okPrefix : okPrefix + " — " + text);
        statusLabel->setText(line);
        if (logEdit)
            logEdit->appendPlainText(line);
        return;
    }
    const QString msg = text.isEmpty() ? QStringLiteral("Request failed.") : text;
    statusLabel->setText(msg);
    if (logEdit)
        logEdit->appendPlainText(msg);
    QMessageBox::warning(this, "X-Coin", msg);
}

void XRewardDividend::refresh()
{
    onRefreshAssets();
    const QString listed = rpc("listsnapshotrequests");
    bool ok = false;
    const QString human = WalletView::humanRpc(listed, &ok);
    if (ok && !human.isEmpty() && logEdit && logEdit->toPlainText().isEmpty())
        logEdit->setPlainText(human);
}

void XRewardDividend::onRefreshAssets()
{
    if (!targetCombo || !payCombo)
        return;
    const QString keepTarget = targetCombo->currentText();
    const QString keepPay = payCombo->currentText();
    targetCombo->clear();
    payCombo->clear();
    payCombo->addItem("XFER");

    UniValue a;
    if (a.read(rpc("listmyassets").toStdString()) && a.isObject() && !a.exists("error")) {
        const std::vector<std::string> keys = a.getKeys();
        for (size_t i = 0; i < keys.size(); ++i) {
            const QString name = QString::fromStdString(keys[i]);
            if (name.endsWith(QLatin1Char('!')))
                continue;
            if (name.contains(QLatin1Char('#')))
                continue;
            targetCombo->addItem(name);
            if (name != QLatin1String("XFER"))
                payCombo->addItem(name);
        }
    }
    if (!keepTarget.isEmpty()) {
        int idx = targetCombo->findText(keepTarget);
        if (idx >= 0)
            targetCombo->setCurrentIndex(idx);
        else
            targetCombo->setEditText(keepTarget);
    }
    if (!keepPay.isEmpty()) {
        int idx = payCombo->findText(keepPay);
        if (idx >= 0)
            payCombo->setCurrentIndex(idx);
        else
            payCombo->setEditText(keepPay);
    }
}

void XRewardDividend::onScheduleSnapshot()
{
    const QString asset = targetCombo->currentText().trimmed();
    if (asset.isEmpty()) {
        QMessageBox::information(this, "X-Coin", "Pick the asset whose holders you want to pay.");
        return;
    }
    QString height = heightEdit->text().trimmed();
    if (height.isEmpty()) {
        bool ok = false;
        const QString tip = WalletView::humanRpc(rpc("getblockcount"), &ok);
        if (!ok) {
            showRpcOutcome(rpc("getblockcount"), QString());
            return;
        }
        height = QString::number(tip.toInt() + 1);
        heightEdit->setText(height);
    }
    showRpcOutcome(rpc("requestsnapshot", QStringList() << asset << height),
                   QString("Snapshot of %1 scheduled at block %2.").arg(asset).arg(height));
}

void XRewardDividend::onPay()
{
    const QString asset = targetCombo->currentText().trimmed();
    const QString pay = payCombo->currentText().trimmed();
    const QString amt = amountEdit->text().trimmed();
    const QString height = heightEdit->text().trimmed();
    if (asset.isEmpty() || pay.isEmpty() || amt.isEmpty() || height.isEmpty()) {
        QMessageBox::information(this, "X-Coin",
            "Need a target asset, a snapshot height, what to pay, and an amount.");
        return;
    }
    const int ans = QMessageBox::question(this, "X-Coin",
        QString("Split %1 %2 among holders of %3 at snapshot %4?\n"
                "The chain must be about 60 blocks past that snapshot.")
            .arg(amt).arg(pay).arg(asset).arg(height),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes)
        return;

    QStringList args;
    args << asset << height << pay << amt;
    const QString excl = excludeEdit->text().trimmed();
    if (!excl.isEmpty())
        args << excl;
    showRpcOutcome(rpc("distributereward", args), "Dividend sent.");
}
