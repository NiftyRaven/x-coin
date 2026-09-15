// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xsend.h"

#include "hostshare.h"
#include "optionsmodel.h"
#include "ravenunits.h"
#include "walletmodel.h"
#include "walletview.h"
#include "xsession.h"
#include "xtheme.h"

static bool looksLikeHandle(const QString& s)
{
    QString t = s.trimmed();
    if (t.startsWith(QLatin1Char('@')))
        return true;
    if (t.size() >= 26 && t.startsWith(QLatin1Char('X')))
        return false;
    if (t.size() >= 1 && t.size() <= 32) {
        for (int i = 0; i < t.size(); ++i) {
            const QChar c = t.at(i);
            if (!c.isLetterOrNumber() && c != QLatin1Char('_'))
                return false;
        }
        return true;
    }
    return false;
}

static QString resolveDestination(const QString& in, QString* displayName, QString* errOut)
{
    const QString t = in.trimmed();
    if (!looksLikeHandle(t))
        return t;
    std::string asset, addr, err;
    if (!hostshare::ResolveHolder(t.toStdString(), asset, addr, err) || addr.empty()) {
        if (errOut)
            *errOut = err.empty() ? QString("That name has not claimed a root yet.") : QString::fromStdString(err);
        return QString();
    }
    if (displayName) {
        QString h = t;
        if (h.startsWith(QLatin1Char('@')))
            h = h.mid(1);
        *displayName = QLatin1Char('@') + h;
    }
    return QString::fromStdString(addr);
}

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

XSend::XSend(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , walletModel(0)
{
    applyTheme();
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 36, 40, 36);
    root->setSpacing(18);

    QLabel* title = new QLabel("SEND");
    title->setObjectName("xsection");
    root->addWidget(title);

    tagLabel = new QLabel("Paste an X address or type @handle.");
    tagLabel->setObjectName("xhint");
    tagLabel->setWordWrap(true);
    root->addWidget(tagLabel);

    balanceLabel = new QLabel;
    balanceLabel->setObjectName("xcard");
    root->addWidget(balanceLabel);

    addrEdit = new QLineEdit;
    addrEdit->setPlaceholderText("@handle or X address");
    root->addWidget(addrEdit);

    QHBoxLayout* pasteRow = new QHBoxLayout;
    QPushButton* paste = new QPushButton("Paste");
    paste->setObjectName("xghost");
    paste->setCursor(Qt::PointingHandCursor);
    pasteRow->addWidget(paste);
    pasteRow->addStretch(1);
    root->addLayout(pasteRow);

    amountEdit = new QLineEdit;
    amountEdit->setPlaceholderText("Amount (XFER)");
    root->addWidget(amountEdit);

    QPushButton* send = new QPushButton("Send");
    send->setObjectName("xprimary");
    send->setMinimumHeight(48);
    send->setCursor(Qt::PointingHandCursor);
    root->addWidget(send);

    statusLabel = new QLabel;
    statusLabel->setObjectName("xhint");
    statusLabel->setWordWrap(true);
    root->addWidget(statusLabel);
    root->addStretch(1);

    connect(paste, SIGNAL(clicked()), this, SLOT(onPaste()));
    connect(send, SIGNAL(clicked()), this, SLOT(onSend()));
}

void XSend::applyTheme()
{
    ApplyXPageTheme(this);
}

void XSend::setWalletModel(WalletModel* model)
{
    walletModel = model;
    refresh();
}

void XSend::setAddress(const QString& addr)
{
    addrEdit->setText(addr);
}

void XSend::refresh()
{
    if (xsession::HasValidSession()) {
        const QString handle = QString::fromStdString(xsession::SignedInHandle());
        tagLabel->setText(QString("Sending from the wallet linked to @%1. Paste @handle or an X address.")
            .arg(handle));
    } else {
        tagLabel->setText("Paste an X address or type @handle.");
    }
    if (!walletModel) {
        balanceLabel->setText("Balance\n(open a wallet)");
        return;
    }
    const int unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : 0;
    if (xsession::HasValidSession()) {
        const QString handle = QString::fromStdString(xsession::SignedInHandle());
        balanceLabel->setText(QString("Available — wallet linked to @%1\n")
            .arg(handle) + RavenUnits::formatWithUnit(unit, walletModel->getBalance()));
    } else {
        balanceLabel->setText(QString("Available\n") + RavenUnits::formatWithUnit(unit, walletModel->getBalance()));
    }
}

QString XSend::rpc(const QString& method, const QStringList& args) const
{
    if (!walletView) return QString();
    return walletView->callRpc(method, args);
}

void XSend::onPaste()
{
    addrEdit->setText(QApplication::clipboard()->text().trimmed());
}

void XSend::onSend()
{
    const QString dest = addrEdit->text().trimmed();
    const QString amt = amountEdit->text().trimmed();
    if (dest.isEmpty() || amt.isEmpty()) {
        QMessageBox::information(this, "X-Coin", "Enter a name or address, and an amount.");
        return;
    }
    QString displayName;
    QString resolveErr;
    const QString resolved = resolveDestination(dest, &displayName, &resolveErr);
    if (resolved.isEmpty()) {
        const QString msg = resolveErr.isEmpty() ? QString("Could not resolve that name.") : resolveErr;
        statusLabel->setText(msg);
        QMessageBox::warning(this, "X-Coin", msg);
        return;
    }
    const QString who = displayName.isEmpty() ? resolved : (displayName + "\n" + resolved);
    if (QMessageBox::question(this, "X-Coin",
            QString("Send %1 XFER to\n%2?").arg(amt).arg(who),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    bool ok = false;
    const QString out = WalletView::humanRpc(rpc("sendtoaddress", QStringList() << resolved << amt), &ok);
    if (ok) {
        statusLabel->setText(QString("Sent.\n%1").arg(out));
        addrEdit->clear();
        amountEdit->clear();
        refresh();
        return;
    }
    const QString msg = out.isEmpty() ? QStringLiteral("Send failed.") : out;
    statusLabel->setText(msg);
    QMessageBox::warning(this, "X-Coin", msg);
}
