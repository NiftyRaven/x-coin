// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/raven-config.h"
#endif

#include "xreceive.h"

#include "addresstablemodel.h"
#include "guiconstants.h"
#include "hostshare.h"
#include "walletmodel.h"
#include "walletview.h"
#include "xsession.h"
#include "xtheme.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

XReceive::XReceive(WalletView* walletViewIn, QWidget* parent)
    : QWidget(parent)
    , walletView(walletViewIn)
    , walletModel(0)
    , useNameBtn(0)
    , invoiceTable(0)
{
    applyTheme();

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* inner = new QWidget;
    QVBoxLayout* root = new QVBoxLayout(inner);
    root->setContentsMargins(40, 36, 40, 36);
    root->setSpacing(14);

    QLabel* title = new QLabel("RECEIVE");
    title->setObjectName("xsection");
    root->addWidget(title);

    handleLabel = new QLabel("—");
    handleLabel->setObjectName("xhero");
    handleLabel->setAlignment(Qt::AlignHCenter);
    root->addWidget(handleLabel);

    tagLabel = new QLabel;
    tagLabel->setObjectName("xhint");
    tagLabel->setWordWrap(true);
    tagLabel->setAlignment(Qt::AlignHCenter);
    root->addWidget(tagLabel);

    qrLabel = new QLabel;
    qrLabel->setAlignment(Qt::AlignHCenter);
    qrLabel->setMinimumHeight(200);
    root->addWidget(qrLabel);

    addressLabel = new QLabel("—");
    addressLabel->setObjectName("xaddr");
    addressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addressLabel->setWordWrap(true);
    addressLabel->setAlignment(Qt::AlignHCenter);
    root->addWidget(addressLabel);

    QHBoxLayout* row = new QHBoxLayout;
    QPushButton* copy = new QPushButton("Copy this address");
    useNameBtn = new QPushButton("Use my name");
    copy->setObjectName("xprimary");
    useNameBtn->setObjectName("xghost");
    copy->setMinimumHeight(44);
    useNameBtn->setMinimumHeight(44);
    copy->setCursor(Qt::PointingHandCursor);
    useNameBtn->setCursor(Qt::PointingHandCursor);
    row->addWidget(copy);
    row->addWidget(useNameBtn);
    root->addLayout(row);

    QLabel* invHead = new QLabel("NEW ADDRESS");
    invHead->setObjectName("xsection");
    root->addWidget(invHead);

    QLabel* invHint = new QLabel(
        "Make a fresh address for one payment or for privacy. "
        "Your @handle still works for people who already know your name.");
    invHint->setObjectName("xhint");
    invHint->setWordWrap(true);
    root->addWidget(invHint);

    labelEdit = new QLineEdit;
    labelEdit->setPlaceholderText("Optional label — e.g. Invoice for Sam");
    root->addWidget(labelEdit);

    QPushButton* neu = new QPushButton("Create new address");
    neu->setObjectName("xprimary");
    neu->setMinimumHeight(48);
    neu->setCursor(Qt::PointingHandCursor);
    root->addWidget(neu);

    invoiceTable = new QTableWidget(0, 2);
    invoiceTable->setHorizontalHeaderLabels(QStringList() << "Label" << "Address");
    invoiceTable->horizontalHeader()->setStretchLastSection(true);
    invoiceTable->verticalHeader()->setVisible(false);
    invoiceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    invoiceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    invoiceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    invoiceTable->setMinimumHeight(180);
    root->addWidget(invoiceTable);

    hintLabel = new QLabel;
    hintLabel->setObjectName("xhint");
    hintLabel->setWordWrap(true);
    root->addWidget(hintLabel);
    root->addStretch(1);

    scroll->setWidget(inner);
    QVBoxLayout* wrap = new QVBoxLayout(this);
    wrap->setContentsMargins(0, 0, 0, 0);
    wrap->addWidget(scroll);

    connect(copy, SIGNAL(clicked()), this, SLOT(onCopy()));
    connect(neu, SIGNAL(clicked()), this, SLOT(onNewAddress()));
    connect(useNameBtn, SIGNAL(clicked()), this, SLOT(onUseName()));
    connect(invoiceTable, SIGNAL(cellClicked(int,int)), this, SLOT(onInvoiceClicked()));
}

void XReceive::applyTheme()
{
    ApplyXPageTheme(this);
}

void XReceive::setWalletModel(WalletModel* model)
{
    walletModel = model;
    refresh();
}

void XReceive::setQr(const QString& payload)
{
#ifdef USE_QRCODE
    if (payload.isEmpty() || payload.length() > MAX_URI_LENGTH) {
        qrLabel->setText("");
        qrLabel->setPixmap(QPixmap());
        return;
    }
    QRcode* code = QRcode_encodeString(payload.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!code) {
        qrLabel->setText("Could not make a QR code.");
        return;
    }
    QImage qrImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
    qrImage.fill(0xffffff);
    unsigned char* p = code->data;
    for (int y = 0; y < code->width; y++) {
        for (int x = 0; x < code->width; x++) {
            qrImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
            p++;
        }
    }
    QRcode_free(code);
    qrLabel->setPixmap(QPixmap::fromImage(qrImage.scaled(220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
#else
    Q_UNUSED(payload);
    qrLabel->setText("");
#endif
}

QString XReceive::nameAddress() const
{
    if (!xsession::HasValidSession())
        return QString();
    const QString handle = QString::fromStdString(xsession::SignedInHandle());
    std::string asset, addr, err;
    if (hostshare::ResolveHolder(handle.toStdString(), asset, addr, err) && !addr.empty())
        return QString::fromStdString(addr);
    return QString();
}

QString XReceive::nextInvoiceLabel() const
{
    const QString typed = labelEdit ? labelEdit->text().trimmed() : QString();
    if (!typed.isEmpty())
        return typed;
    int n = 1;
    if (walletModel && walletModel->getAddressTableModel()) {
        AddressTableModel* m = walletModel->getAddressTableModel();
        const QModelIndex parent;
        for (int r = 0; r < m->rowCount(parent); ++r) {
            if (m->index(r, 0, parent).data(AddressTableModel::TypeRole).toString() != AddressTableModel::Receive)
                continue;
            const QString lab = m->index(r, AddressTableModel::Label, parent).data().toString();
            if (lab.startsWith(QLatin1String("Invoice")))
                n++;
        }
    }
    return n <= 1 ? QString("Invoice") : QString("Invoice %1").arg(n);
}

void XReceive::showAddress(const QString& addr, const QString& why)
{
    currentAddress = addr;
    addressLabel->setText(addr.isEmpty() ? QString("(open a wallet)") : addr);
    addressLabel->setVisible(true);
    setQr(addr);
    if (addr.isEmpty())
        hintLabel->setText("Open a wallet first.");
    else
        hintLabel->setText(why);
}

void XReceive::fillInvoiceTable()
{
    if (!invoiceTable)
        return;
    invoiceTable->setRowCount(0);
    if (!walletModel || !walletModel->getAddressTableModel())
        return;
    AddressTableModel* m = walletModel->getAddressTableModel();
    const QModelIndex parent;
    int row = 0;
    for (int r = 0; r < m->rowCount(parent); ++r) {
        if (m->index(r, 0, parent).data(AddressTableModel::TypeRole).toString() != AddressTableModel::Receive)
            continue;
        const QString lab = m->index(r, AddressTableModel::Label, parent).data().toString();
        const QString addr = m->index(r, AddressTableModel::Address, parent).data().toString();
        invoiceTable->insertRow(row);
        QString shown = lab;
        if (shown.isEmpty())
            shown = "(no label)";
        if (!nameCardAddress.isEmpty() && addr == nameCardAddress)
            shown = "Name card  ·  " + shown;
        invoiceTable->setItem(row, 0, new QTableWidgetItem(shown));
        invoiceTable->setItem(row, 1, new QTableWidgetItem(addr));
        if (addr == selectedAddress)
            invoiceTable->selectRow(row);
        row++;
    }
}

void XReceive::refresh()
{
    QString handle;
    nameCardAddress = nameAddress();
    if (xsession::HasValidSession()) {
        handle = QString::fromStdString(xsession::SignedInHandle());
        handleLabel->setText(handle.isEmpty() ? QString("—") : ("@" + handle));
        tagLabel->setText(nameCardAddress.isEmpty()
            ? "Claim your name on Home, or create an invoice address below."
            : "People can pay @" + handle + ". Or create a new address for one payment.");
    } else {
        handleLabel->setText("This wallet");
        tagLabel->setText("Create a labeled address, copy it, or show the QR. Sign in with X to put a name on the card.");
    }
    if (useNameBtn)
        useNameBtn->setVisible(!nameCardAddress.isEmpty());

    fillInvoiceTable();

    QString keep = selectedAddress;
    if (keep.isEmpty() && !nameCardAddress.isEmpty())
        keep = nameCardAddress;
    if (keep.isEmpty() && invoiceTable && invoiceTable->rowCount() > 0)
        keep = invoiceTable->item(0, 1)->text();

    if (keep.isEmpty() && walletModel && walletModel->getAddressTableModel()) {
        AddressTableModel* m = walletModel->getAddressTableModel();
        keep = m->addRow(AddressTableModel::Receive, "Receive", "");
        if (keep.isEmpty())
            reportAddRowError();
        fillInvoiceTable();
    }

    selectedAddress = keep;
    if (!nameCardAddress.isEmpty() && selectedAddress == nameCardAddress)
        showAddress(selectedAddress, handle.isEmpty()
            ? QString("This is the address for your name card.")
            : QString("This is the address for @%1. Copy sends this address.").arg(handle));
    else if (!selectedAddress.isEmpty())
        showAddress(selectedAddress, "This invoice address is selected. Copy or show the QR. Your name (if claimed) still works.");
    else
        showAddress(QString(), "Open a wallet first.");
}

void XReceive::reportAddRowError()
{
    if (!walletModel || !walletModel->getAddressTableModel()) {
        QMessageBox::warning(this, "X-Coin", "Open a wallet first.");
        return;
    }
    switch (walletModel->getAddressTableModel()->getEditStatus()) {
    case AddressTableModel::WALLET_UNLOCK_FAILURE:
        QMessageBox::warning(this, "X-Coin", "Unlock the wallet to make a new address.");
        break;
    case AddressTableModel::KEY_GENERATION_FAILURE:
        QMessageBox::warning(this, "X-Coin", "Could not make a new key.");
        break;
    default:
        QMessageBox::warning(this, "X-Coin", "Could not make a new address.");
        break;
    }
}

void XReceive::onCopy()
{
    if (currentAddress.isEmpty())
        refresh();
    if (currentAddress.isEmpty()) {
        QMessageBox::warning(this, "X-Coin", "Open a wallet first, then copy a receive address.");
        return;
    }
    QApplication::clipboard()->setText(currentAddress);
    hintLabel->setText("Copied.");
}

void XReceive::onNewAddress()
{
    if (!walletModel || !walletModel->getAddressTableModel()) {
        QMessageBox::warning(this, "X-Coin", "Open a wallet first.");
        return;
    }
    const QString label = nextInvoiceLabel();
    const QString addr = walletModel->getAddressTableModel()->addRow(AddressTableModel::Receive, label, "");
    if (addr.isEmpty()) {
        reportAddRowError();
        return;
    }
    selectedAddress = addr;
    if (labelEdit)
        labelEdit->clear();
    fillInvoiceTable();
    showAddress(addr, QString("New address “%1” is ready. Copy it or show this QR. Your @handle still works.")
        .arg(label));
}

void XReceive::onUseName()
{
    if (nameCardAddress.isEmpty())
        nameCardAddress = nameAddress();
    if (nameCardAddress.isEmpty()) {
        QMessageBox::information(this, "X-Coin", "Claim your name on Home first. Until then, use a new address below.");
        return;
    }
    selectedAddress = nameCardAddress;
    refresh();
}

void XReceive::onInvoiceClicked()
{
    if (!invoiceTable)
        return;
    const int r = invoiceTable->currentRow();
    if (r < 0)
        return;
    QTableWidgetItem* it = invoiceTable->item(r, 1);
    if (!it)
        return;
    selectedAddress = it->text();
    const QString lab = invoiceTable->item(r, 0) ? invoiceTable->item(r, 0)->text() : QString();
    showAddress(selectedAddress, QString("Showing “%1”. Copy this address or the QR.").arg(lab));
}
