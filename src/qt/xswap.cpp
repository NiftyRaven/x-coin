// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xswap.h"

#include "addresstablemodel.h"
#include "walletmodel.h"
#include "walletview.h"
#include "xtheme.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <univalue.h>

static const char* kXfer = "XFER";

static bool isCoin(const QString& asset)
{
    return asset.isEmpty() || asset.compare(QLatin1String(kXfer), Qt::CaseInsensitive) == 0;
}

static QString fmtAmt(double d)
{
    QString s = QString::number(d, 'f', 8);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.'))))
        s.chop(1);
    if (s.isEmpty())
        s = QLatin1String("0");
    return s;
}

static double asDouble(const UniValue& v)
{
    if (v.isNum())
        return v.get_real();
    bool ok = false;
    const double d = QString::fromStdString(v.getValStr()).toDouble(&ok);
    return ok ? d : 0;
}

static UniValue amountValue(const QString& s)
{
    bool ok = false;
    const double d = s.trimmed().toDouble(&ok);
    if (ok && std::fabs(d - std::floor(d + 1e-12)) < 1e-9)
        return UniValue(static_cast<int64_t>(d + 0.1));
    return UniValue(s.trimmed().toStdString());
}

static UniValue transferOut(const QString& asset, const QString& amt)
{
    UniValue inner(UniValue::VOBJ);
    inner.pushKV(asset.toStdString(), amountValue(amt));
    UniValue wrap(UniValue::VOBJ);
    wrap.pushKV("transfer", inner);
    return wrap;
}

static bool readJson(const QString& raw, UniValue& u, QString* err)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        if (err)
            *err = QString("Could not read that.");
        return false;
    }
    if (!trimmed.startsWith(QLatin1Char('{')) && !trimmed.startsWith(QLatin1Char('['))) {
        u.setStr(trimmed.toStdString());
        return true;
    }
    if (!u.read(trimmed.toStdString())) {
        if (err)
            *err = QString("Could not read that.");
        return false;
    }
    if (u.isObject() && u.exists("error")) {
        const UniValue& e = u["error"];
        QString msg;
        if (e.isObject() && e.exists("message"))
            msg = QString::fromStdString(e["message"].getValStr());
        else
            msg = QString::fromStdString(e.write());
        if (err)
            *err = msg.isEmpty() ? QString("Request failed.") : msg;
        return false;
    }
    return true;
}

struct PickedCoin {
    QString txid;
    int vout;
    double amount;
};

struct PickedAsset {
    QString txid;
    int vout;
    double amount;
    QString name;
};

static QTableWidget* makeTable(const QStringList& headers)
{
    QTableWidget* t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(180);
    t->setShowGrid(false);
    t->setAlternatingRowColors(false);
    return t;
}

static void setRow(QTableWidget* table, int row, const QString& id, const QStringList& cols)
{
    for (int c = 0; c < cols.size(); ++c) {
        QTableWidgetItem* item = new QTableWidgetItem(cols.at(c));
        if (c == 0)
            item->setData(Qt::UserRole, id);
        table->setItem(row, c, item);
    }
}

XSwap::XSwap(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , walletModel(0)
    , bookTable(0)
    , mineTable(0)
    , holdTable(0)
    , listPrice(0)
    , bookEmpty(0)
    , statusLabel(0)
    , otcToggle(0)
    , otcWidget(0)
    , giveAsset(0)
    , wantAsset(0)
    , giveAmt(0)
    , wantAmt(0)
    , feeEdit(0)
    , offerBox(0)
    , signedBox(0)
    , previewLabel(0)
{
    applyTheme();

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* inner = new QWidget;
    QVBoxLayout* root = new QVBoxLayout(inner);
    root->setContentsMargins(40, 36, 40, 36);
    root->setSpacing(14);

    QLabel* title = new QLabel("MARKET");
    title->setObjectName("xsection");
    root->addWidget(title);

    QLabel* intro = new QLabel(
        "List assets. Other wallets buy them with XFER. No copy-paste, no shop server — "
        "listings travel peer to peer. Each row is one bag. Select several to list more than one "
        "(subassets, uniques, or split bags). Send to yourself first if you only want to sell part of a bag.");
    intro->setObjectName("xhint");
    intro->setWordWrap(true);
    root->addWidget(intro);

    QLabel* forSale = new QLabel("FOR SALE");
    forSale->setObjectName("xsection");
    root->addWidget(forSale);

    bookEmpty = new QLabel("Nothing listed yet. Stay connected. When someone lists, it shows up here.");
    bookEmpty->setObjectName("xhint");
    bookEmpty->setWordWrap(true);
    root->addWidget(bookEmpty);

    bookTable = makeTable(QStringList() << "Asset" << "Amount" << "Price (XFER)");
    root->addWidget(bookTable);

    QPushButton* buyBtn = new QPushButton("Buy with XFER");
    buyBtn->setObjectName("xprimary");
    buyBtn->setMinimumHeight(48);
    buyBtn->setCursor(Qt::PointingHandCursor);
    root->addWidget(buyBtn);

    QLabel* sellHead = new QLabel("YOUR BAGS");
    sellHead->setObjectName("xsection");
    root->addWidget(sellHead);

    QLabel* sellHint = new QLabel("Select one or more. Ctrl-click to pick several. Same XFER price for each listing.");
    sellHint->setObjectName("xhint");
    sellHint->setWordWrap(true);
    root->addWidget(sellHint);

    holdTable = makeTable(QStringList() << "Asset" << "Amount");
    holdTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    holdTable->setMinimumHeight(160);
    root->addWidget(holdTable);

    QHBoxLayout* listRow = new QHBoxLayout;
    listPrice = new QLineEdit;
    listPrice->setPlaceholderText("Price in XFER (each)");
    QPushButton* listBtn = new QPushButton("List selected");
    listBtn->setObjectName("xprimary");
    listBtn->setCursor(Qt::PointingHandCursor);
    listBtn->setMinimumHeight(40);
    listRow->addWidget(listPrice, 1);
    listRow->addWidget(listBtn);
    root->addLayout(listRow);

    QLabel* mineHead = new QLabel("YOUR LISTINGS");
    mineHead->setObjectName("xsection");
    root->addWidget(mineHead);

    mineTable = makeTable(QStringList() << "Asset" << "Amount" << "Price (XFER)");
    mineTable->setMinimumHeight(120);
    root->addWidget(mineTable);

    QPushButton* cancelBtn = new QPushButton("Cancel listing");
    cancelBtn->setObjectName("xghost");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    root->addWidget(cancelBtn);

    statusLabel = new QLabel;
    statusLabel->setObjectName("xhint");
    statusLabel->setWordWrap(true);
    root->addWidget(statusLabel);

    otcToggle = new QPushButton("Private trade (copy and paste)");
    otcToggle->setObjectName("xghost");
    otcToggle->setCursor(Qt::PointingHandCursor);
    otcToggle->setCheckable(true);
    root->addWidget(otcToggle);

    otcWidget = new QWidget;
    otcWidget->setVisible(false);
    QVBoxLayout* otc = new QVBoxLayout(otcWidget);
    otc->setContentsMargins(0, 8, 0, 0);
    otc->setSpacing(14);

    QLabel* otcHint = new QLabel(
        "Offline two-party swap. You both sign one transaction. Use this when the other person is not on the book.");
    otcHint->setObjectName("xhint");
    otcHint->setWordWrap(true);
    otc->addWidget(otcHint);

    QLabel* s1 = new QLabel("1  ·  YOU START");
    s1->setObjectName("xsection");
    otc->addWidget(s1);

    QLabel* giveLbl = new QLabel("You give");
    giveLbl->setObjectName("xhint");
    otc->addWidget(giveLbl);
    QHBoxLayout* giveRow = new QHBoxLayout;
    giveAsset = new QComboBox;
    giveAsset->setEditable(true);
    giveAsset->addItem(kXfer);
    giveAmt = new QLineEdit;
    giveAmt->setPlaceholderText("Amount");
    giveRow->addWidget(giveAsset, 2);
    giveRow->addWidget(giveAmt, 1);
    otc->addLayout(giveRow);

    QLabel* wantLbl = new QLabel("You get");
    wantLbl->setObjectName("xhint");
    otc->addWidget(wantLbl);
    QHBoxLayout* wantRow = new QHBoxLayout;
    wantAsset = new QComboBox;
    wantAsset->setEditable(true);
    wantAsset->addItem(kXfer);
    wantAmt = new QLineEdit;
    wantAmt->setPlaceholderText("Amount");
    wantRow->addWidget(wantAsset, 2);
    wantRow->addWidget(wantAmt, 1);
    otc->addLayout(wantRow);

    QHBoxLayout* feeRow = new QHBoxLayout;
    QLabel* feeLbl = new QLabel("Network fee you pay");
    feeLbl->setObjectName("xhint");
    feeEdit = new QLineEdit;
    feeEdit->setText("0.01");
    feeEdit->setMaximumWidth(100);
    feeRow->addWidget(feeLbl);
    feeRow->addWidget(feeEdit);
    feeRow->addStretch(1);
    otc->addLayout(feeRow);

    QPushButton* makeBtn = new QPushButton("Make offer");
    makeBtn->setObjectName("xprimary");
    makeBtn->setMinimumHeight(48);
    makeBtn->setCursor(Qt::PointingHandCursor);
    otc->addWidget(makeBtn);

    offerBox = new QPlainTextEdit;
    offerBox->setPlaceholderText("Your offer appears here. Send this text to the other person.");
    offerBox->setMinimumHeight(120);
    otc->addWidget(offerBox);

    QPushButton* copyOffer = new QPushButton("Copy offer");
    copyOffer->setObjectName("xghost");
    copyOffer->setCursor(Qt::PointingHandCursor);
    otc->addWidget(copyOffer);

    QLabel* s2 = new QLabel("2  ·  THEY ACCEPT  (or you accept theirs)");
    s2->setObjectName("xsection");
    otc->addWidget(s2);

    QLabel* accHint = new QLabel("Paste their offer, check the preview, then sign.");
    accHint->setObjectName("xhint");
    accHint->setWordWrap(true);
    otc->addWidget(accHint);

    previewLabel = new QLabel;
    previewLabel->setObjectName("xcard");
    previewLabel->setWordWrap(true);
    previewLabel->setText("Paste an offer to see what you give and what you get.");
    otc->addWidget(previewLabel);

    QPushButton* acceptBtn = new QPushButton("Sign their offer");
    acceptBtn->setObjectName("xprimary");
    acceptBtn->setMinimumHeight(48);
    acceptBtn->setCursor(Qt::PointingHandCursor);
    otc->addWidget(acceptBtn);

    signedBox = new QPlainTextEdit;
    signedBox->setPlaceholderText("Half-signed transaction appears here. Send this back to the person who started.");
    signedBox->setMinimumHeight(100);
    otc->addWidget(signedBox);

    QPushButton* copySigned = new QPushButton("Copy signed transaction");
    copySigned->setObjectName("xghost");
    copySigned->setCursor(Qt::PointingHandCursor);
    otc->addWidget(copySigned);

    QLabel* s3 = new QLabel("3  ·  YOU FINISH");
    s3->setObjectName("xsection");
    otc->addWidget(s3);

    QLabel* finHint = new QLabel("Paste the signed transaction they sent back. This wallet adds your signature and broadcasts.");
    finHint->setObjectName("xhint");
    finHint->setWordWrap(true);
    otc->addWidget(finHint);

    QPushButton* finishBtn = new QPushButton("Sign and broadcast");
    finishBtn->setObjectName("xprimary");
    finishBtn->setMinimumHeight(48);
    finishBtn->setCursor(Qt::PointingHandCursor);
    otc->addWidget(finishBtn);

    root->addWidget(otcWidget);
    root->addStretch(1);

    scroll->setWidget(inner);
    QVBoxLayout* wrap = new QVBoxLayout(this);
    wrap->setContentsMargins(0, 0, 0, 0);
    wrap->addWidget(scroll);

    connect(buyBtn, SIGNAL(clicked()), this, SLOT(onBuy()));
    connect(bookTable, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(onBuy()));
    connect(listBtn, SIGNAL(clicked()), this, SLOT(onList()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    connect(otcToggle, SIGNAL(clicked()), this, SLOT(onToggleOtc()));
    connect(makeBtn, SIGNAL(clicked()), this, SLOT(onMakeOffer()));
    connect(acceptBtn, SIGNAL(clicked()), this, SLOT(onAcceptOffer()));
    connect(finishBtn, SIGNAL(clicked()), this, SLOT(onFinish()));
    connect(copyOffer, SIGNAL(clicked()), this, SLOT(onCopyOffer()));
    connect(copySigned, SIGNAL(clicked()), this, SLOT(onCopySigned()));

    QTimer* poll = new QTimer(this);
    connect(poll, SIGNAL(timeout()), this, SLOT(onPollBook()));
    poll->start(4000);
}

void XSwap::applyTheme()
{
    ApplyXPageTheme(this);
}

void XSwap::setWalletModel(WalletModel* model)
{
    walletModel = model;
    refresh();
}

QString XSwap::rpc(const QString& method, const QStringList& args) const
{
    if (!walletView)
        return QString();
    return walletView->callRpc(method, args);
}

bool XSwap::rpcOk(const QString& method, const QStringList& args, QString* out, QString* err) const
{
    const QString raw = rpc(method, args);
    UniValue u;
    QString e;
    if (!readJson(raw, u, &e)) {
        if (err)
            *err = e;
        return false;
    }
    if (out)
        *out = raw;
    return true;
}

void XSwap::showStatus(const QString& text, bool err, bool popup)
{
    statusLabel->setObjectName(err ? "xstatuserr" : "xhint");
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
    statusLabel->setText(text);
    if (popup && err && !text.isEmpty())
        QMessageBox::warning(this, "X-Coin", text);
}

QString XSwap::comboAsset(QComboBox* box) const
{
    if (!box)
        return kXfer;
    const QString t = box->currentText().trimmed();
    return t.isEmpty() ? QString(kXfer) : t;
}

QString XSwap::newLabeledAddress(const QString& label)
{
    if (walletModel && walletModel->getAddressTableModel()) {
        const QString addr = walletModel->getAddressTableModel()->addRow(AddressTableModel::Receive, label, "");
        if (!addr.isEmpty())
            return addr;
    }
    QString err;
    UniValue u;
    if (readJson(rpc("getnewaddress"), u, &err) && u.isStr())
        return QString::fromStdString(u.get_str());
    return QString();
}

void XSwap::fillAssetCombos()
{
    const QString keepGive = comboAsset(giveAsset);
    const QString keepWant = comboAsset(wantAsset);
    giveAsset->clear();
    wantAsset->clear();
    giveAsset->addItem(kXfer);
    wantAsset->addItem(kXfer);
    UniValue a;
    QString err;
    if (readJson(rpc("listmyassets"), a, &err) && a.isObject()) {
        const std::vector<std::string> keys = a.getKeys();
        for (size_t i = 0; i < keys.size(); ++i) {
            const QString name = QString::fromStdString(keys[i]);
            if (name.contains(QLatin1Char('~')) || name.endsWith(QLatin1Char('!')))
                continue;
            giveAsset->addItem(name);
            wantAsset->addItem(name);
        }
    }
    int gi = giveAsset->findText(keepGive);
    if (gi >= 0)
        giveAsset->setCurrentIndex(gi);
    else
        giveAsset->setEditText(keepGive);
    int wi = wantAsset->findText(keepWant);
    if (wi >= 0)
        wantAsset->setCurrentIndex(wi);
    else
        wantAsset->setEditText(keepWant);
    fillHoldings();
}

void XSwap::fillHoldings()
{
    if (!holdTable)
        return;
    QSet<QString> listed;
    UniValue book;
    QString err;
    if (readJson(rpc("listmarket"), book, &err) && book.isArray()) {
        for (size_t i = 0; i < book.size(); ++i) {
            const UniValue& row = book[i];
            if (!row.isObject() || !row.exists("mine") || !row["mine"].isTrue())
                continue;
            const QString txid = QString::fromStdString(row["asset_txid"].getValStr());
            const QString vout = QString::number(row["asset_vout"].get_int());
            listed.insert(txid + QLatin1Char(':') + vout);
        }
    }

    UniValue a;
    if (!readJson(rpc("listmyassets", QStringList() << "*" << "true"), a, &err) || !a.isObject()) {
        holdTable->setRowCount(0);
        return;
    }

    holdTable->setRowCount(0);
    int rows = 0;
    const std::vector<std::string> keys = a.getKeys();
    for (size_t i = 0; i < keys.size(); ++i) {
        const QString name = QString::fromStdString(keys[i]);
        if (name.contains(QLatin1Char('~')) || name.endsWith(QLatin1Char('!')))
            continue;
        const UniValue& rec = a[keys[i]];
        if (!rec.isObject() || !rec.exists("outpoints") || !rec["outpoints"].isArray())
            continue;
        const UniValue& ops = rec["outpoints"];
        for (size_t o = 0; o < ops.size(); ++o) {
            const UniValue& op = ops[o];
            if (!op.isObject())
                continue;
            const QString txid = QString::fromStdString(op["txid"].getValStr());
            const int vout = op["vout"].get_int();
            if (listed.contains(txid + QLatin1Char(':') + QString::number(vout)))
                continue;
            const QString amount = fmtAmt(asDouble(op["amount"]));
            holdTable->insertRow(rows);
            QTableWidgetItem* assetItem = new QTableWidgetItem(name);
            assetItem->setData(Qt::UserRole, txid);
            assetItem->setData(Qt::UserRole + 1, vout);
            holdTable->setItem(rows, 0, assetItem);
            holdTable->setItem(rows, 1, new QTableWidgetItem(amount));
            ++rows;
        }
    }
}

QString XSwap::selectedId(QTableWidget* table) const
{
    if (!table)
        return QString();
    const int row = table->currentRow();
    if (row < 0)
        return QString();
    QTableWidgetItem* item = table->item(row, 0);
    if (!item)
        return QString();
    return item->data(Qt::UserRole).toString();
}

void XSwap::fillBook()
{
    UniValue a;
    QString err;
    if (!readJson(rpc("listmarket"), a, &err) || !a.isArray()) {
        showStatus(err.isEmpty() ? QString("Could not read the book.") : err, true, false);
        return;
    }

    const QString keepBook = selectedId(bookTable);
    const QString keepMine = selectedId(mineTable);
    bookTable->setRowCount(0);
    mineTable->setRowCount(0);
    int bookRows = 0;
    int mineRows = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        const UniValue& row = a[i];
        if (!row.isObject())
            continue;
        const QString id = QString::fromStdString(row["id"].getValStr());
        const QString asset = QString::fromStdString(row["asset"].getValStr());
        const QString amount = fmtAmt(asDouble(row["amount"]));
        const QString price = fmtAmt(asDouble(row["price"]));
        const bool mine = row.exists("mine") && row["mine"].isTrue();
        const QStringList cols = QStringList() << asset << amount << price;
        if (mine) {
            mineTable->insertRow(mineRows);
            setRow(mineTable, mineRows, id, cols);
            ++mineRows;
        } else {
            bookTable->insertRow(bookRows);
            setRow(bookTable, bookRows, id, cols);
            ++bookRows;
        }
    }
    bookEmpty->setVisible(bookRows == 0);
    bookTable->setVisible(bookRows > 0);
    if (!keepBook.isEmpty()) {
        for (int r = 0; r < bookTable->rowCount(); ++r) {
            if (bookTable->item(r, 0) && bookTable->item(r, 0)->data(Qt::UserRole).toString() == keepBook) {
                bookTable->selectRow(r);
                break;
            }
        }
    }
    if (!keepMine.isEmpty()) {
        for (int r = 0; r < mineTable->rowCount(); ++r) {
            if (mineTable->item(r, 0) && mineTable->item(r, 0)->data(Qt::UserRole).toString() == keepMine) {
                mineTable->selectRow(r);
                break;
            }
        }
    }
}

void XSwap::refresh()
{
    fillAssetCombos();
    fillBook();
}

void XSwap::onPollBook()
{
    if (!isVisible())
        return;
    fillBook();
}

void XSwap::onToggleOtc()
{
    const bool on = otcToggle->isChecked();
    otcWidget->setVisible(on);
    otcToggle->setText(on ? "Hide private trade" : "Private trade (copy and paste)");
}

void XSwap::onBuy()
{
    const QString id = selectedId(bookTable);
    if (id.isEmpty()) {
        showStatus("Select a listing first.", true);
        return;
    }
    const int row = bookTable->currentRow();
    const QString asset = bookTable->item(row, 0) ? bookTable->item(row, 0)->text() : QString();
    const QString amount = bookTable->item(row, 1) ? bookTable->item(row, 1)->text() : QString();
    const QString price = bookTable->item(row, 2) ? bookTable->item(row, 2)->text() : QString();
    if (QMessageBox::question(this, "X-Coin",
            QString("Buy %1 %2 for %3 XFER?\nYou also pay a 0.01 XFER network fee.")
                .arg(amount).arg(asset).arg(price),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!walletModel) {
        showStatus("Open a wallet first.", true);
        return;
    }
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        showStatus("Unlock the wallet first.", true);
        return;
    }
    QString err;
    UniValue u;
    if (!readJson(rpc("takeask", QStringList() << id), u, &err) || !u.isObject()) {
        showStatus(err.isEmpty() ? QString("Could not buy that.") : err, true);
        return;
    }
    const QString txid = QString::fromStdString(u.exists("txid") ? u["txid"].getValStr() : "");
    showStatus(txid.isEmpty() ? QString("Bought.") : QString("Bought. %1").arg(txid), false);
    fillBook();
}

void XSwap::onList()
{
    if (!holdTable) {
        showStatus("Pick an asset to list.", true);
        return;
    }
    QList<int> rows;
    const QList<QTableWidgetItem*> selected = holdTable->selectedItems();
    for (int i = 0; i < selected.size(); ++i) {
        const int r = selected.at(i)->row();
        if (!rows.contains(r))
            rows.append(r);
    }
    if (rows.isEmpty()) {
        showStatus("Select one or more bags to list.", true);
        return;
    }
    const QString price = listPrice ? listPrice->text().trimmed() : QString();
    bool ok = false;
    if (price.toDouble(&ok) <= 0 || !ok) {
        showStatus("Enter a price in XFER.", true);
        return;
    }
    QStringList names;
    for (int i = 0; i < rows.size(); ++i) {
        QTableWidgetItem* item = holdTable->item(rows.at(i), 0);
        const QString n = item ? item->text() : QString();
        if (!n.isEmpty() && !names.contains(n))
            names.append(n);
    }
    const QString confirm = rows.size() == 1
        ? QString("List %1 for %2 XFER?").arg(names.join(", ")).arg(price)
        : QString("List %1 bags for %2 XFER each?\n%3")
            .arg(rows.size()).arg(price).arg(names.join(", "));
    if (QMessageBox::question(this, "X-Coin", confirm,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!walletModel) {
        showStatus("Open a wallet first.", true);
        return;
    }
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        showStatus("Unlock the wallet first.", true);
        return;
    }

    int listed = 0;
    QString lastErr;
    for (int i = 0; i < rows.size(); ++i) {
        QTableWidgetItem* item = holdTable->item(rows.at(i), 0);
        if (!item)
            continue;
        const QString asset = item->text();
        const QString txid = item->data(Qt::UserRole).toString();
        const int vout = item->data(Qt::UserRole + 1).toInt();
        QString err;
        UniValue u;
        if (!readJson(rpc("postask", QStringList() << asset << price << txid << QString::number(vout)), u, &err)
            || !u.isObject()) {
            lastErr = err.isEmpty() ? QString("Could not list %1.").arg(asset) : err;
            continue;
        }
        ++listed;
    }
    if (listed == 0) {
        showStatus(lastErr.isEmpty() ? QString("Could not list that.") : lastErr, true);
        return;
    }
    if (listed < rows.size())
        showStatus(QString("Listed %1 of %2 at %3 XFER each. %4")
            .arg(listed).arg(rows.size()).arg(price).arg(lastErr), true, false);
    else
        showStatus(QString("Listed %1 bag(s) at %2 XFER each. Other wallets can buy them from the book.")
            .arg(listed).arg(price), false);
    fillHoldings();
    fillBook();
}

void XSwap::onCancel()
{
    const QString id = selectedId(mineTable);
    if (id.isEmpty()) {
        showStatus("Select one of your listings first.", true);
        return;
    }
    if (QMessageBox::question(this, "X-Coin",
            "Take this listing down? The asset comes back to this wallet. You pay 0.01 XFER.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!walletModel) {
        showStatus("Open a wallet first.", true);
        return;
    }
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        showStatus("Unlock the wallet first.", true);
        return;
    }
    QString err;
    UniValue u;
    if (!readJson(rpc("cancelask", QStringList() << id), u, &err)) {
        showStatus(err.isEmpty() ? QString("Could not cancel.") : err, true);
        return;
    }
    showStatus("Listing cancelled.", false);
    fillHoldings();
    fillBook();
}

static bool pickCoin(WalletView* view, double need, PickedCoin& out, QString* err)
{
    UniValue a;
    if (!readJson(view->callRpc("listunspent"), a, err) || !a.isArray()) {
        if (err && err->isEmpty())
            *err = "Could not list spendable XFER.";
        return false;
    }
    double best = 1e99;
    bool found = false;
    for (size_t i = 0; i < a.size(); ++i) {
        const UniValue& row = a[i];
        if (!row.isObject())
            continue;
        if (row.exists("spendable") && !row["spendable"].isTrue())
            continue;
        const double amt = asDouble(row["amount"]);
        if (amt + 1e-8 < need)
            continue;
        if (amt < best) {
            best = amt;
            out.txid = QString::fromStdString(row["txid"].getValStr());
            out.vout = row["vout"].get_int();
            out.amount = amt;
            found = true;
        }
    }
    if (!found) {
        if (err)
            *err = QString("Need one spendable XFER output of at least %1. Send to yourself first if the coins are split.")
                .arg(fmtAmt(need));
        return false;
    }
    return true;
}

static bool pickAsset(WalletView* view, const QString& name, double need, PickedAsset& out, QString* err)
{
    UniValue a;
    if (!readJson(view->callRpc("listmyassets", QStringList() << name << "true"), a, err) || !a.isObject()) {
        if (err && err->isEmpty())
            *err = "Could not list that asset.";
        return false;
    }
    if (!a.exists(name.toStdString())) {
        if (err)
            *err = QString("This wallet does not hold %1.").arg(name);
        return false;
    }
    const UniValue& rec = a[name.toStdString()];
    if (!rec.isObject() || !rec.exists("outpoints") || !rec["outpoints"].isArray()) {
        if (err)
            *err = QString("No outpoints for %1.").arg(name);
        return false;
    }
    const UniValue& ops = rec["outpoints"];
    double best = 1e99;
    bool found = false;
    for (size_t i = 0; i < ops.size(); ++i) {
        const UniValue& row = ops[i];
        const double amt = asDouble(row["amount"]);
        if (amt + 1e-8 < need)
            continue;
        if (amt < best) {
            best = amt;
            out.txid = QString::fromStdString(row["txid"].getValStr());
            out.vout = row["vout"].get_int();
            out.amount = amt;
            out.name = name;
            found = true;
        }
    }
    if (!found) {
        if (err)
            *err = QString("Need one %1 output of at least %2. Combine first if it is split.")
                .arg(name).arg(fmtAmt(need));
        return false;
    }
    return true;
}

static void pushIn(UniValue& arr, const QString& txid, int vout)
{
    UniValue o(UniValue::VOBJ);
    o.pushKV("txid", txid.toStdString());
    o.pushKV("vout", vout);
    arr.push_back(o);
}

void XSwap::onMakeOffer()
{
    const QString give = comboAsset(giveAsset);
    const QString want = comboAsset(wantAsset);
    const QString gAmt = giveAmt->text().trimmed();
    const QString wAmt = wantAmt->text().trimmed();
    const QString feeS = feeEdit->text().trimmed().isEmpty() ? QString("0.01") : feeEdit->text().trimmed();
    bool okG = false, okW = false, okF = false;
    const double gd = gAmt.toDouble(&okG);
    const double wd = wAmt.toDouble(&okW);
    const double fee = feeS.toDouble(&okF);
    if (!okG || !okW || gd <= 0 || wd <= 0) {
        showStatus("Enter what you give and what you get.", true);
        return;
    }
    if (!okF || fee < 0) {
        showStatus("Fee must be a number.", true);
        return;
    }
    if (give.compare(want, Qt::CaseInsensitive) == 0) {
        showStatus("Give and get have to be different.", true);
        return;
    }
    if (!walletView) {
        showStatus("Open a wallet first.", true);
        return;
    }

    UniValue inputs(UniValue::VARR);
    QString err;
    double giveIn = 0;
    double feeIn = 0;
    if (isCoin(give)) {
        PickedCoin c;
        if (!pickCoin(walletView, gd + fee, c, &err)) {
            showStatus(err, true);
            return;
        }
        pushIn(inputs, c.txid, c.vout);
        giveIn = c.amount;
        feeIn = c.amount;
    } else {
        PickedAsset a;
        if (!pickAsset(walletView, give, gd, a, &err)) {
            showStatus(err, true);
            return;
        }
        pushIn(inputs, a.txid, a.vout);
        giveIn = a.amount;
        PickedCoin c;
        if (!pickCoin(walletView, fee, c, &err)) {
            showStatus(err, true);
            return;
        }
        pushIn(inputs, c.txid, c.vout);
        feeIn = c.amount;
    }

    const QString recv = newLabeledAddress("Swap receive");
    const QString coinCh = newLabeledAddress("Swap coin change");
    const QString assetCh = newLabeledAddress("Swap asset change");
    if (recv.isEmpty() || coinCh.isEmpty() || assetCh.isEmpty()) {
        showStatus("Could not make swap addresses. Unlock the wallet and try again.", true);
        return;
    }

    UniValue offer(UniValue::VOBJ);
    offer.pushKV("xswap", 1);
    UniValue g(UniValue::VOBJ);
    g.pushKV("asset", isCoin(give) ? kXfer : give.toStdString());
    g.pushKV("amount", gAmt.toStdString());
    UniValue w(UniValue::VOBJ);
    w.pushKV("asset", isCoin(want) ? kXfer : want.toStdString());
    w.pushKV("amount", wAmt.toStdString());
    offer.pushKV("give", g);
    offer.pushKV("want", w);
    offer.pushKV("inputs", inputs);
    offer.pushKV("receive", recv.toStdString());
    offer.pushKV("coin_change", coinCh.toStdString());
    offer.pushKV("asset_change", assetCh.toStdString());
    offer.pushKV("fee", feeS.toStdString());
    offer.pushKV("give_in", fmtAmt(giveIn).toStdString());
    offer.pushKV("fee_in", fmtAmt(feeIn).toStdString());

    const QString text = QString::fromStdString(offer.write(2));
    offerBox->setPlainText(text);
    QApplication::clipboard()->setText(text);
    previewLabel->setText(QString("You give %1 %2.\nYou get %3 %4.\nFee %5 XFER (you pay).\nOffer copied.")
        .arg(gAmt).arg(isCoin(give) ? kXfer : give)
        .arg(wAmt).arg(isCoin(want) ? kXfer : want)
        .arg(feeS));
    showStatus("Offer copied. Send it to the other person. They sign, then you finish here.", false);
}

void XSwap::onAcceptOffer()
{
    UniValue offer;
    QString err;
    if (!readJson(offerBox->toPlainText(), offer, &err) || !offer.isObject()) {
        showStatus(err.isEmpty() ? QString("Paste their offer in the box above.") : err, true);
        return;
    }
    if (!offer.exists("xswap") || !offer.exists("give") || !offer.exists("want") || !offer.exists("inputs")) {
        showStatus("That is not a swap offer.", true);
        return;
    }
    if (!offer.exists("give_in") || !offer.exists("fee_in")) {
        showStatus("This offer is from an older wallet. Ask them to Make offer again.", true);
        return;
    }

    const UniValue& g = offer["give"];
    const UniValue& w = offer["want"];
    const QString theyGive = QString::fromStdString(g["asset"].getValStr());
    const QString theyWant = QString::fromStdString(w["asset"].getValStr());
    const QString theyGiveAmt = QString::fromStdString(g["amount"].getValStr());
    const QString theyWantAmt = QString::fromStdString(w["amount"].getValStr());
    const QString feeS = offer.exists("fee") ? QString::fromStdString(offer["fee"].getValStr()) : QString("0.01");
    const QString makerRecv = QString::fromStdString(offer["receive"].getValStr());
    const QString makerCoinCh = QString::fromStdString(offer["coin_change"].getValStr());
    const QString makerAssetCh = offer.exists("asset_change")
        ? QString::fromStdString(offer["asset_change"].getValStr())
        : makerCoinCh;
    const double fee = feeS.toDouble();
    const double giveIn = asDouble(offer["give_in"]);
    const double feeIn = asDouble(offer["fee_in"]);
    const double needGive = theyWantAmt.toDouble();

    previewLabel->setText(QString(
        "They give you %1 %2.\nYou give them %3 %4.\nThey pay the %5 XFER fee.")
        .arg(theyGiveAmt).arg(theyGive)
        .arg(theyWantAmt).arg(theyWant)
        .arg(feeS));

    if (QMessageBox::question(this, "X-Coin",
            QString("Sign this swap?\n\nYou give %1 %2\nYou get %3 %4")
                .arg(theyWantAmt).arg(theyWant)
                .arg(theyGiveAmt).arg(theyGive),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    UniValue inputs = offer["inputs"];
    if (!inputs.isArray()) {
        showStatus("Offer inputs are missing.", true);
        return;
    }

    double takerIn = 0;
    if (isCoin(theyWant)) {
        PickedCoin c;
        if (!pickCoin(walletView, needGive, c, &err)) {
            showStatus(err, true);
            return;
        }
        pushIn(inputs, c.txid, c.vout);
        takerIn = c.amount;
    } else {
        PickedAsset a;
        if (!pickAsset(walletView, theyWant, needGive, a, &err)) {
            showStatus(err, true);
            return;
        }
        pushIn(inputs, a.txid, a.vout);
        takerIn = a.amount;
    }

    const QString takerRecv = newLabeledAddress("Swap receive");
    const QString takerCoinCh = newLabeledAddress("Swap coin change");
    const QString takerAssetCh = newLabeledAddress("Swap asset change");
    if (takerRecv.isEmpty() || takerCoinCh.isEmpty() || takerAssetCh.isEmpty()) {
        showStatus("Could not make swap addresses. Unlock the wallet and try again.", true);
        return;
    }

    UniValue outs(UniValue::VOBJ);
    const bool makerGiveCoin = isCoin(theyGive);
    const bool takerGiveCoin = isCoin(theyWant);

    if (makerGiveCoin) {
        const double leftover = giveIn - theyGiveAmt.toDouble() - fee;
        if (leftover > 1e-8)
            outs.pushKV(makerCoinCh.toStdString(), amountValue(fmtAmt(leftover)));
        outs.pushKV(takerRecv.toStdString(), amountValue(theyGiveAmt));
    } else {
        const double feeLeft = feeIn - fee;
        if (feeLeft > 1e-8)
            outs.pushKV(makerCoinCh.toStdString(), amountValue(fmtAmt(feeLeft)));
    }
    if (takerGiveCoin) {
        outs.pushKV(makerRecv.toStdString(), amountValue(theyWantAmt));
        const double tLeft = takerIn - needGive;
        if (tLeft > 1e-8)
            outs.pushKV(takerCoinCh.toStdString(), amountValue(fmtAmt(tLeft)));
    }

    if (!makerGiveCoin) {
        outs.pushKV(takerRecv.toStdString(), transferOut(theyGive, theyGiveAmt));
        if (giveIn - theyGiveAmt.toDouble() > 1e-8)
            outs.pushKV(makerAssetCh.toStdString(), transferOut(theyGive, fmtAmt(giveIn - theyGiveAmt.toDouble())));
    }
    if (!takerGiveCoin) {
        outs.pushKV(makerRecv.toStdString(), transferOut(theyWant, theyWantAmt));
        if (takerIn - needGive > 1e-8)
            outs.pushKV(takerAssetCh.toStdString(), transferOut(theyWant, fmtAmt(takerIn - needGive)));
    }

    QString rawTx;
    if (!rpcOk("createrawtransaction",
               QStringList() << QString::fromStdString(inputs.write()) << QString::fromStdString(outs.write()),
               &rawTx, &err)) {
        showStatus(err, true);
        return;
    }
    UniValue created;
    if (!readJson(rawTx, created, &err)) {
        showStatus(err, true);
        return;
    }
    const QString hex = created.isStr() ? QString::fromStdString(created.get_str()) : rawTx.trimmed().remove('\n').remove(' ');

    const QString signedRaw = rpc("signrawtransaction", QStringList() << hex);
    UniValue sig;
    if (!readJson(signedRaw, sig, &err) || !sig.isObject() || !sig.exists("hex")) {
        showStatus(err.isEmpty() ? QString("Could not sign.") : err, true);
        return;
    }
    const QString signedHex = QString::fromStdString(sig["hex"].getValStr());
    signedBox->setPlainText(signedHex);
    QApplication::clipboard()->setText(signedHex);
    showStatus("Signed. Copied. Send that text back so they can finish.", false);
}

void XSwap::onFinish()
{
    QString hex = signedBox->toPlainText().trimmed();
    if (hex.isEmpty())
        hex = offerBox->toPlainText().trimmed();
    if (hex.isEmpty() || hex.startsWith(QLatin1Char('{'))) {
        showStatus("Paste the signed transaction they sent back.", true);
        return;
    }
    hex.remove('\n').remove(' ').remove('\r');

    QString err;
    UniValue dec;
    if (readJson(rpc("decoderawtransaction", QStringList() << hex), dec, &err) && dec.isObject()) {
        const QString vin = dec.exists("vin") && dec["vin"].isArray()
            ? QString::number((int)dec["vin"].size())
            : QString("?");
        const QString vout = dec.exists("vout") && dec["vout"].isArray()
            ? QString::number((int)dec["vout"].size())
            : QString("?");
        previewLabel->setText(QString("About to sign and broadcast a swap with %1 inputs and %2 outputs.")
            .arg(vin).arg(vout));
    }

    if (QMessageBox::question(this, "X-Coin",
            "Sign the rest of this swap and broadcast it?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    const QString signedRaw = rpc("signrawtransaction", QStringList() << hex);
    UniValue sig;
    if (!readJson(signedRaw, sig, &err) || !sig.isObject() || !sig.exists("hex")) {
        showStatus(err.isEmpty() ? QString("Could not sign.") : err, true);
        return;
    }
    if (sig.exists("complete") && !sig["complete"].isTrue()) {
        showStatus("This wallet could not finish the signatures. The other person may still need to sign, or an input is missing.", true);
        return;
    }
    const QString doneHex = QString::fromStdString(sig["hex"].getValStr());
    QString sent;
    if (!rpcOk("sendrawtransaction", QStringList() << doneHex, &sent, &err)) {
        showStatus(err, true);
        return;
    }
    UniValue tx;
    QString txid = sent.trimmed();
    if (readJson(sent, tx, &err) && tx.isStr())
        txid = QString::fromStdString(tx.get_str());
    showStatus(QString("Broadcast. %1").arg(txid), false);
}

void XSwap::onCopyOffer()
{
    const QString t = offerBox->toPlainText().trimmed();
    if (t.isEmpty()) {
        showStatus("Make an offer first, or paste one.", true);
        return;
    }
    QApplication::clipboard()->setText(t);
    showStatus("Copied.", false);
}

void XSwap::onCopySigned()
{
    const QString t = signedBox->toPlainText().trimmed();
    if (t.isEmpty()) {
        showStatus("Nothing to copy yet.", true);
        return;
    }
    QApplication::clipboard()->setText(t);
    showStatus("Copied.", false);
}
