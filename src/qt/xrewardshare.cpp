// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xrewardshare.h"

#include "amount.h"
#include "hostshare.h"
#include "walletview.h"
#include "xtheme.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <univalue.h>

XRewardShare::XRewardShare(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , shareChk(0)
    , pctEdit(0)
    , handleEdit(0)
    , previewLabel(0)
    , statusLabel(0)
    , guestTable(0)
{
    applyTheme();

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* inner = new QWidget;
    QVBoxLayout* root = new QVBoxLayout(inner);
    root->setContentsMargins(40, 36, 40, 36);
    root->setSpacing(16);

    QLabel* title = new QLabel("LOTTERY SHARE");
    title->setObjectName("xsection");
    root->addWidget(title);

    QLabel* hint = new QLabel(
        "Give a percent of your mature lottery win to people you invite by name. "
        "They do not enter the draw. This is a send from this wallet after a win matures.");
    hint->setObjectName("xhint");
    hint->setWordWrap(true);
    root->addWidget(hint);

    shareChk = new QCheckBox("Share lottery wins");
    shareChk->setObjectName("xcheck");
    shareChk->setCursor(Qt::PointingHandCursor);
    root->addWidget(shareChk);

    QHBoxLayout* pctRow = new QHBoxLayout;
    QLabel* pctLbl = new QLabel("Guest percent");
    pctLbl->setObjectName("xhint");
    pctEdit = new QLineEdit;
    pctEdit->setPlaceholderText("20");
    pctEdit->setMaxLength(3);
    pctEdit->setFixedWidth(72);
    QLabel* pctSign = new QLabel("% of this wallet’s payout that minute");
    pctSign->setObjectName("xhint");
    pctSign->setWordWrap(true);
    pctRow->addWidget(pctLbl);
    pctRow->addWidget(pctEdit);
    pctRow->addWidget(pctSign, 1);
    root->addLayout(pctRow);

    previewLabel = new QLabel;
    previewLabel->setObjectName("xcard");
    previewLabel->setWordWrap(true);
    root->addWidget(previewLabel);

    QLabel* guestsHead = new QLabel("GUESTS");
    guestsHead->setObjectName("xsection");
    root->addWidget(guestsHead);

    guestTable = new QTableWidget(0, 3);
    guestTable->setHorizontalHeaderLabels(QStringList() << "Handle" << "Root" << "Ready");
    guestTable->horizontalHeader()->setStretchLastSection(true);
    guestTable->verticalHeader()->setVisible(false);
    guestTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    guestTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    guestTable->setMinimumHeight(160);
    root->addWidget(guestTable);

    QHBoxLayout* guestRow = new QHBoxLayout;
    handleEdit = new QLineEdit;
    handleEdit->setPlaceholderText("guest handle — @ optional");
    QPushButton* addBtn = new QPushButton("Add");
    addBtn->setObjectName("xghost");
    addBtn->setCursor(Qt::PointingHandCursor);
    QPushButton* removeBtn = new QPushButton("Remove");
    removeBtn->setObjectName("xghost");
    removeBtn->setCursor(Qt::PointingHandCursor);
    guestRow->addWidget(handleEdit, 1);
    guestRow->addWidget(addBtn);
    guestRow->addWidget(removeBtn);
    root->addLayout(guestRow);

    statusLabel = new QLabel;
    statusLabel->setObjectName("xhint");
    statusLabel->setWordWrap(true);
    root->addWidget(statusLabel);
    root->addStretch(1);

    scroll->setWidget(inner);
    QVBoxLayout* wrap = new QVBoxLayout(this);
    wrap->setContentsMargins(0, 0, 0, 0);
    wrap->addWidget(scroll);

    connect(shareChk, SIGNAL(toggled(bool)), this, SLOT(onShareToggled(bool)));
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAddGuest()));
    connect(removeBtn, SIGNAL(clicked()), this, SLOT(onRemoveGuest()));
    connect(pctEdit, SIGNAL(editingFinished()), this, SLOT(onPercentEdited()));

    QTimer* t = new QTimer(this);
    connect(t, SIGNAL(timeout()), this, SLOT(refresh()));
    t->start(5000);
    refresh();
}

void XRewardShare::applyTheme()
{
    ApplyXPageTheme(this);
}

QString XRewardShare::rpc(const QString& method, const QStringList& args) const
{
    if (!walletView) return QString();
    return walletView->callRpc(method, args);
}

void XRewardShare::showRpcOutcome(const QString& raw, const QString& okPrefix)
{
    bool ok = false;
    const QString text = WalletView::humanRpc(raw, &ok);
    statusLabel->setObjectName(ok ? "xhint" : "xstatuserr");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
    if (ok) {
        statusLabel->setText(okPrefix.isEmpty() ? text : (text.isEmpty() ? okPrefix : okPrefix + " — " + text));
        return;
    }
    const QString msg = text.isEmpty() ? QStringLiteral("Request failed.") : text;
    statusLabel->setText(msg);
    QMessageBox::warning(this, "X-Coin", msg);
}

void XRewardShare::refresh()
{
    UniValue g;
    if (!g.read(rpc("listguests").toStdString()) || !g.isObject() || g.exists("error")) {
        previewLabel->setText("Open a wallet, then Sign in with X (verified) to share a win.");
        return;
    }

    const bool on = g["enabled"].isTrue();
    const int pct = g["guest_percent"].isNum() ? g["guest_percent"].get_int() : 20;
    const bool indexed = g["assetindex"].isTrue();

    shareChk->blockSignals(true);
    shareChk->setChecked(on);
    shareChk->blockSignals(false);
    if (pctEdit && !pctEdit->hasFocus()) {
        pctEdit->blockSignals(true);
        pctEdit->setText(QString::number(pct));
        pctEdit->blockSignals(false);
    }

    int ready = 0;
    guestTable->setRowCount(0);
    if (g.exists("guests") && g["guests"].isArray()) {
        const UniValue& arr = g["guests"];
        guestTable->setRowCount((int)arr.size());
        for (size_t i = 0; i < arr.size(); i++) {
            const UniValue& row = arr[i];
            const QString h = QString::fromStdString(row["handle"].getValStr());
            const bool isReady = row["ready"].isTrue();
            if (isReady) ready++;
            QString asset;
            if (row.exists("asset"))
                asset = QString::fromStdString(row["asset"].getValStr());
            guestTable->setItem((int)i, 0, new QTableWidgetItem("@" + h));
            guestTable->setItem((int)i, 1, new QTableWidgetItem(asset));
            guestTable->setItem((int)i, 2, new QTableWidgetItem(isReady ? "yes" : "no holder yet"));
        }
    }

    if (!indexed) {
        previewLabel->setText(
            "Asset index is off. Turn sharing on once to write assetindex=1 and restart. "
            "Then add handles that have claimed a root.");
    } else if (!on) {
        previewLabel->setText("Sharing is off. Ready guests will not be paid.");
    } else {
        const CAmount example = 1250 * COIN;
        const CAmount pot = hostshare::GuestPot(example, pct);
        QString each = "—";
        if (ready > 0)
            each = QString::number((double)hostshare::GuestShare(pot, ready) / COIN, 'f', 2);
        previewLabel->setText(QString(
            "If this wallet’s mature win is 1250 XFER, guests split %1 XFER.\n"
            "%2 ready guest(s) → about %3 XFER each.\n"
            "Payouts show in Activity after a win matures.")
            .arg(QString::number((double)pot / COIN, 'f', 2))
            .arg(ready)
            .arg(each));
    }
}

void XRewardShare::onShareToggled(bool on)
{
    if (on) {
        bool indexed = false;
        UniValue cur;
        if (cur.read(rpc("listguests").toStdString()) && cur.isObject() && !cur.exists("error"))
            indexed = cur["assetindex"].isTrue();
        if (!indexed) {
            const int ans = QMessageBox::question(this, "X-Coin",
                "Sharing looks up each guest’s root address. That needs assetindex=1 "
                "and a one-time restart that rebuilds indexes.\n\nWrite the setting and restart after this?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (ans != QMessageBox::Yes) {
                shareChk->blockSignals(true);
                shareChk->setChecked(false);
                shareChk->blockSignals(false);
                return;
            }
            showRpcOutcome(rpc("enableassetindex"),
                           "Wrote assetindex=1. Close this wallet and open it again. Then turn sharing on.");
            shareChk->blockSignals(true);
            shareChk->setChecked(false);
            shareChk->blockSignals(false);
            return;
        }
        showRpcOutcome(rpc("sethostshare", QStringList() << "true"), "Sharing lottery wins with guests.");
    } else {
        showRpcOutcome(rpc("sethostshare", QStringList() << "false"), "Stopped sharing lottery wins.");
    }
    refresh();
}

void XRewardShare::onAddGuest()
{
    const QString handle = handleEdit->text().trimmed();
    if (handle.isEmpty()) {
        QMessageBox::information(this, "X-Coin", "Type the guest’s X handle first.");
        return;
    }
    showRpcOutcome(rpc("inviteguest", QStringList() << handle), "Guest added.");
    refresh();
}

void XRewardShare::onRemoveGuest()
{
    QString handle = handleEdit->text().trimmed();
    if (handle.isEmpty() && guestTable->currentRow() >= 0) {
        QTableWidgetItem* it = guestTable->item(guestTable->currentRow(), 0);
        if (it)
            handle = it->text();
    }
    if (handle.isEmpty()) {
        QMessageBox::information(this, "X-Coin", "Type or select the guest handle to remove.");
        return;
    }
    showRpcOutcome(rpc("removeguest", QStringList() << handle), "Guest removed.");
    refresh();
}

void XRewardShare::onPercentEdited()
{
    bool ok = false;
    const int pct = pctEdit->text().trimmed().toInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, "X-Coin", "Guest percent must be a number from 1 to 100.");
        return;
    }
    showRpcOutcome(rpc("setguestpercent", QStringList() << QString::number(pct)),
                   QString("Guests will split %1%% of each mature win.").arg(pct));
    refresh();
}
