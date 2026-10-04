// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "ravenunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "transactionfilterproxy.h"
#include "transactiontablemodel.h"
#include "assetfilterproxy.h"
#include "assettablemodel.h"
#include "walletmodel.h"
#include "assetrecord.h"
#include "lottery.h"
#include "xprice.h"
#include "xpriceclient.h"
#include "xcoinsend.h"
#include "walletview.h"

#include <QAbstractItemDelegate>
#include <QDateTime>
#include <QPainter>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QShowEvent>
#include <QSizePolicy>
#include <validation.h>
#include <utiltime.h>

#define DECORATION_SIZE 54
#define NUM_ITEMS 8

#include <QDebug>
#include <QTimer>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QScrollBar>
#include <QUrl>
#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define QTversionPreFiveEleven
#endif

static int TextWidth(const QFontMetrics& fm, const QString& text)
{
#if defined(QTversionPreFiveEleven)
    return fm.width(text);
#else
    return fm.horizontalAdvance(text);
#endif
}

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle *_platformStyle, QObject *parent=nullptr):
        QAbstractItemDelegate(parent), unit(RavenUnits::RVN),
        platformStyle(_platformStyle)
    {
    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const bool dark = darkModeEnabled;
        const QColor text = dark ? QColor("#fafafa") : QColor("#18181b");
        const QColor muted = dark ? QColor("#a1a1aa") : QColor("#71717a");
        const QColor line = dark ? QColor("#27272a") : QColor("#e4e4e7");
        const QColor negative = dark ? QColor("#f87171") : QColor("#b91c1c");
        const QColor pending = dark ? QColor("#d4d4d8") : QColor("#52525b");

        QRect row = option.rect.adjusted(4, 2, -8, -2);
        painter->setPen(QPen(line, 1));
        painter->drawLine(row.bottomLeft(), row.bottomRight());

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QString amountText = index.data(TransactionTableModel::FormattedAmountRole).toString();
        QString assetName = index.data(TransactionTableModel::AssetNameRole).toString();
        if (!confirmed)
            amountText = QString("Pending  ") + amountText;

        QColor amountColor = text;
        if (amount < 0)
            amountColor = negative;
        else if (!confirmed)
            amountColor = pending;

        QFont dateFont = GUIUtil::getSubLabelFont();
        dateFont.setPixelSize(12);
        QFont amountFont = GUIUtil::getSubLabelFont();
        amountFont.setPixelSize(14);
        amountFont.setWeight(QFont::DemiBold);
        QFont addrFont = GUIUtil::getSubLabelFont();
        addrFont.setPixelSize(13);

        const int pad = 8;
        QRect top(row.left() + pad, row.top() + 6, row.width() - pad * 2, row.height() / 2 - 2);
        QRect bottom(row.left() + pad, row.center().y() - 2, row.width() - pad * 2, row.height() / 2 - 4);

        painter->setFont(amountFont);
        const int amountWidth = TextWidth(painter->fontMetrics(), amountText);
        painter->setPen(amountColor);
        painter->drawText(top, Qt::AlignRight | Qt::AlignVCenter, amountText);

        painter->setFont(dateFont);
        painter->setPen(muted);
        painter->drawText(QRect(top.left(), top.top(), top.width() - amountWidth - 12, top.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        QString meta = assetName;
        painter->setFont(addrFont);
        const int metaWidth = meta.isEmpty() ? 0 : TextWidth(painter->fontMetrics(), meta) + 12;
        QString shown = painter->fontMetrics().elidedText(address, Qt::ElideMiddle, qMax(40, bottom.width() - metaWidth));
        painter->setPen(text);
        painter->drawText(QRect(bottom.left(), bottom.top(), bottom.width() - metaWidth, bottom.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, shown);
        if (!meta.isEmpty()) {
            painter->setPen(muted);
            painter->drawText(bottom, Qt::AlignRight | Qt::AlignVCenter, meta);
        }

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(280, 58);
    }

    int unit;
    const PlatformStyle *platformStyle;

};

class AssetViewDelegate : public QAbstractItemDelegate
{
Q_OBJECT
public:
    explicit AssetViewDelegate(const PlatformStyle *_platformStyle, QObject *parent=nullptr):
            QAbstractItemDelegate(parent), unit(RavenUnits::RVN),
            platformStyle(_platformStyle)
    {
    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const bool dark = darkModeEnabled;
        const QColor card = dark ? QColor("#161616") : QColor("#fafafa");
        const QColor border = dark ? QColor("#2e2e2e") : QColor("#e4e4e7");
        const QColor text = dark ? QColor("#fafafa") : QColor("#18181b");
        const QColor muted = dark ? QColor("#a1a1aa") : QColor("#71717a");

        QRect cardRect = option.rect.adjusted(2, 4, -10, -4);
        QPainterPath path;
        path.addRoundedRect(cardRect, 10, 10);
        painter->fillPath(path, card);
        painter->setPen(QPen(border, 1));
        painter->drawPath(path);

        const QString name = index.data(AssetTableModel::AssetNameRole).toString();
        const QString amountText = index.data(AssetTableModel::FormattedAmountRole).toString();
        const bool admin = index.data(AssetTableModel::AdministratorRole).toBool();
        const QString ipfs = index.data(AssetTableModel::AssetIPFSHashRole).toString();

        QString meta;
        if (admin)
            meta = QStringLiteral("Admin");
        if (ipfs.startsWith(QLatin1String("Qm"))) {
            if (!meta.isEmpty())
                meta += QStringLiteral("  ·  ");
            meta += QStringLiteral("IPFS");
        }

        QFont amountFont;
#if !defined(Q_OS_MAC)
        amountFont.setFamily("Open Sans");
#endif
        amountFont.setPixelSize(14);
        amountFont.setWeight(QFont::DemiBold);

        QFont nameFont;
#if !defined(Q_OS_MAC)
        nameFont.setFamily("Open Sans");
#endif
        nameFont.setPixelSize(15);
        nameFont.setWeight(QFont::Medium);

        painter->setFont(amountFont);
        const int amountWidth = TextWidth(painter->fontMetrics(), amountText);
        const int left = cardRect.left() + 16;
        const int rightPad = 16;
        const int nameWidth = qMax(40, cardRect.width() - 32 - amountWidth - 16);

        painter->setPen(text);
        painter->drawText(QRect(cardRect.right() - rightPad - amountWidth, cardRect.top(), amountWidth, cardRect.height()),
                          Qt::AlignRight | Qt::AlignVCenter, amountText);

        painter->setFont(nameFont);
        const QString shown = painter->fontMetrics().elidedText(name, Qt::ElideRight, nameWidth);
        if (meta.isEmpty()) {
            painter->setPen(text);
            painter->drawText(QRect(left, cardRect.top(), nameWidth, cardRect.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, shown);
        } else {
            painter->setPen(text);
            painter->drawText(QRect(left, cardRect.top() + 6, nameWidth, cardRect.height() / 2),
                              Qt::AlignLeft | Qt::AlignVCenter, shown);
            QFont metaFont = nameFont;
            metaFont.setPixelSize(11);
            metaFont.setWeight(QFont::Normal);
            painter->setFont(metaFont);
            painter->setPen(muted);
            painter->drawText(QRect(left, cardRect.center().y() - 2, nameWidth, cardRect.height() / 2),
                              Qt::AlignLeft | Qt::AlignVCenter, meta);
        }

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(280, 58);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include "overviewpage.moc"
#include "ravengui.h"
#include <QFontDatabase>

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    currentWatchOnlyBalance(-1),
    currentWatchUnconfBalance(-1),
    currentWatchImmatureBalance(-1),
    txdelegate(new TxViewDelegate(platformStyle, this)),
    assetdelegate(new AssetViewDelegate(platformStyle, this)),
    lotteryTitle(0),
    lotteryStatus(0),
    lotteryDetail(0),
    usdtLabel(0),
    assetEmpty(0),
    coinSummary(0),
    sendStatus(0),
    sendDest(0),
    sendAmount(0),
    coinControlCheck(0),
    chooseCoinsBtn(0),
    priceClient(0),
    balanceStyle(platformStyle)
{
    ui->setupUi(this);

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);
    ui->labelAssetStatus->setIcon(icon);

    // Lottery status card (X theme)
    QFrame *lotteryFrame = new QFrame(this);
    lotteryFrame->setObjectName("lotteryFrame");
    lotteryFrame->setStyleSheet(QString(".QFrame {background-color: %1; padding: 12px;}").arg(platformStyle->WidgetBackGroundColor().name()));
    lotteryFrame->setGraphicsEffect(GUIUtil::getShadowEffect());
    QVBoxLayout *lotteryLay = new QVBoxLayout(lotteryFrame);
    lotteryLay->setContentsMargins(8, 8, 8, 8);
    lotteryLay->setSpacing(6);
    lotteryTitle = new QLabel(tr("Lottery"), lotteryFrame);
    lotteryTitle->setStyleSheet(STRING_LABEL_COLOR);
    lotteryTitle->setFont(GUIUtil::getTopLabelFont());
    lotteryStatus = new QLabel(tr("Waiting for node…"), lotteryFrame);
    lotteryStatus->setFont(GUIUtil::getSubLabelFontBolded());
    lotteryStatus->setWordWrap(true);
    lotteryDetail = new QLabel(QStringLiteral("—"), lotteryFrame);
    lotteryDetail->setFont(GUIUtil::getSubLabelFont());
    lotteryDetail->setWordWrap(true);
    lotteryLay->addWidget(lotteryTitle);
    lotteryLay->addWidget(lotteryStatus);
    lotteryLay->addWidget(lotteryDetail);
    ui->verticalLayout_2->insertWidget(1, lotteryFrame);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(0, 0));
    ui->listTransactions->setMinimumHeight(160);
    ui->listTransactions->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->listTransactions->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    /** Create the list of assets */
    ui->listAssets->setItemDelegate(assetdelegate);
    ui->listAssets->setIconSize(QSize(0, 0));
    ui->listAssets->setMinimumHeight(120);
    ui->listAssets->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->listAssets->viewport()->setAutoFillBackground(false);

    // Delay before filtering assetes in ms
    static const int input_filter_delay = 200;

    QTimer *asset_typing_delay;
    asset_typing_delay = new QTimer(this);
    asset_typing_delay->setSingleShot(true);
    asset_typing_delay->setInterval(input_filter_delay);
    connect(ui->assetSearch, SIGNAL(textChanged(QString)), asset_typing_delay, SLOT(start()));
    connect(asset_typing_delay, SIGNAL(timeout()), this, SLOT(assetSearchChanged()));

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));
    ui->listAssets->viewport()->installEventFilter(this);

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    connect(ui->labelAssetStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));

    usdtLabel = new QLabel(ui->frame);
    usdtLabel->setObjectName("usdtLabel");
    usdtLabel->setWordWrap(true);
    usdtLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    usdtLabel->hide();
    ui->verticalLayout_4->addWidget(usdtLabel);

    assetEmpty = new QLabel(ui->assetFrame);
    assetEmpty->setObjectName("assetEmpty");
    assetEmpty->setAlignment(Qt::AlignCenter);
    assetEmpty->setWordWrap(true);
    assetEmpty->setMinimumHeight(72);
    assetEmpty->hide();
    ui->verticalLayout_5->addWidget(assetEmpty);

    buildSendCard();

    priceClient = new XPriceClient(this);
    priceClient->setOnUpdate([this]() { updateUsdt(); });

    /** Update the labels font */
    ui->rvnBalancesLabel->setFont(GUIUtil::getTopLabelFont());
    ui->assetBalanceLabel->setFont(GUIUtil::getTopLabelFont());
    ui->recentTransactionsLabel->setFont(GUIUtil::getTopLabelFont());

    /** Update the sub labels font */
    ui->labelBalanceText->setFont(GUIUtil::getSubLabelFont());
    ui->labelPendingText->setFont(GUIUtil::getSubLabelFont());
    ui->labelImmatureText->setFont(GUIUtil::getSubLabelFont());
    ui->labelSpendable->setFont(GUIUtil::getSubLabelFont());
    ui->labelWatchonly->setFont(GUIUtil::getSubLabelFont());
    ui->labelBalance->setFont(GUIUtil::getSubLabelFont());
    ui->labelUnconfirmed->setFont(GUIUtil::getSubLabelFont());
    ui->labelImmature->setFont(GUIUtil::getSubLabelFont());
    ui->labelWatchAvailable->setFont(GUIUtil::getSubLabelFont());
    ui->labelWatchPending->setFont(GUIUtil::getSubLabelFont());
    ui->labelWatchImmature->setFont(GUIUtil::getSubLabelFont());
    ui->labelTotalText->setFont(GUIUtil::getSubLabelFont());
    ui->labelTotal->setFont(GUIUtil::getTopLabelFontBolded());
    ui->labelWatchTotal->setFont(GUIUtil::getTopLabelFontBolded());

    ui->assetSearch->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->assetSearch->setAlignment(Qt::AlignVCenter);
    ui->assetSearch->setClearButtonEnabled(true);

    // Trigger the call to show the assets table if assets are active
    showAssets();

    // context menu actions
    sendAction = new QAction(tr("Send Asset"), this);
    QAction *copyAmountAction = new QAction(tr("Copy Amount"), this);
    QAction *copyNameAction = new QAction(tr("Copy Name"), this);
    copyHashAction = new QAction(tr("Copy Hash"), this);
    issueSub = new QAction(tr("Issue Sub Asset"), this);
    issueUnique = new QAction(tr("Issue Unique Asset"), this);
    reissue = new QAction(tr("Reissue Asset"), this);
    openURL = new QAction(tr("Open IPFS in Browser"), this);

    sendAction->setObjectName("Send");
    issueSub->setObjectName("Sub");
    issueUnique->setObjectName("Unique");
    reissue->setObjectName("Reissue");
    copyNameAction->setObjectName("Copy Name");
    copyAmountAction->setObjectName("Copy Amount");
    copyHashAction->setObjectName("Copy Hash");
    openURL->setObjectName("Browse");

    // context menu
    contextMenu = new QMenu(this);
    contextMenu->addAction(sendAction);
    contextMenu->addAction(issueSub);
    contextMenu->addAction(issueUnique);
    contextMenu->addAction(reissue);
    contextMenu->addSeparator();
    contextMenu->addAction(openURL);
    contextMenu->addAction(copyHashAction);
    contextMenu->addSeparator();
    contextMenu->addAction(copyNameAction);
    contextMenu->addAction(copyAmountAction);

    applyChrome();
}

bool OverviewPage::eventFilter(QObject *object, QEvent *event)
{
    // If the asset viewport is being clicked
    if (object == ui->listAssets->viewport() && event->type() == QEvent::MouseButtonPress) {

        // Grab the mouse event
        QMouseEvent * mouseEv = static_cast<QMouseEvent*>(event);

        // Select the current index at the mouse location
        QModelIndex currentIndex = ui->listAssets->indexAt(mouseEv->pos());

        // Open the menu on right click, direct url on left click
        if (mouseEv->buttons() & Qt::RightButton ) {
            handleAssetRightClicked(currentIndex);
        } else if (mouseEv->buttons() & Qt::LeftButton) {
            openIPFSForAsset(currentIndex);
        }
    }

    return QWidget::eventFilter(object, event);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleAssetRightClicked(const QModelIndex &index)
{
    if(assetFilter) {
        // Grab the data elements from the index that we need to disable and enable menu items
        QString name = index.data(AssetTableModel::AssetNameRole).toString();
        QString ipfshash = index.data(AssetTableModel::AssetIPFSHashRole).toString();
        QString ipfsbrowser = walletModel->getOptionsModel()->getIpfsUrl();

        if (IsAssetNameAnOwner(name.toStdString())) {
            name = name.left(name.size() - 1);
            sendAction->setDisabled(true);
        } else {
            sendAction->setDisabled(false);
        }

        // If the ipfs hash isn't there or doesn't start with Qm, disable the action item
        if (ipfshash.count() > 0 && ipfshash.indexOf("Qm") == 0 && ipfsbrowser.indexOf("http") == 0 ) {
            openURL->setDisabled(false);
        } else {
            openURL->setDisabled(true);
        }

        if (ipfshash.count() > 0) {
            copyHashAction->setDisabled(false);
        } else {
            copyHashAction->setDisabled(true);
        }

        if (!index.data(AssetTableModel::AdministratorRole).toBool()) {
            issueSub->setDisabled(true);
            issueUnique->setDisabled(true);
            reissue->setDisabled(true);
        } else {
            issueSub->setDisabled(false);
            issueUnique->setDisabled(false);
            reissue->setDisabled(true);
            CNewAsset asset;
            auto currentActiveAssetCache = GetCurrentAssetCache();
            if (currentActiveAssetCache && currentActiveAssetCache->GetAssetMetaDataIfExists(name.toStdString(), asset))
                if (asset.nReissuable)
                    reissue->setDisabled(false);

        }

        QAction* action = contextMenu->exec(QCursor::pos());

        if (action) {
            if (action->objectName() == "Send")
                Q_EMIT assetSendClicked(assetFilter->mapToSource(index));
            else if (action->objectName() == "Sub")
                Q_EMIT assetIssueSubClicked(assetFilter->mapToSource(index));
            else if (action->objectName() == "Unique")
                Q_EMIT assetIssueUniqueClicked(assetFilter->mapToSource(index));
            else if (action->objectName() == "Reissue")
                Q_EMIT assetReissueClicked(assetFilter->mapToSource(index));
            else if (action->objectName() == "Copy Name")
                GUIUtil::setClipboard(index.data(AssetTableModel::AssetNameRole).toString());
            else if (action->objectName() == "Copy Amount")
                GUIUtil::setClipboard(index.data(AssetTableModel::FormattedAmountRole).toString());
            else if (action->objectName() == "Copy Hash")
                GUIUtil::setClipboard(ipfshash);
            else if (action->objectName() == "Browse") {
                QDesktopServices::openUrl(QUrl::fromUserInput(ipfsbrowser.replace("%s", ipfshash)));
            }
        }
    }
}

void OverviewPage::handleOutOfSyncWarningClicks()
{
    Q_EMIT outOfSyncWarningClicked();
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    currentWatchOnlyBalance = watchOnlyBalance;
    currentWatchUnconfBalance = watchUnconfBalance;
    currentWatchImmatureBalance = watchImmatureBalance;
    ui->labelBalance->setText(RavenUnits::formatWithUnit(unit, balance, false, RavenUnits::separatorAlways));
    ui->labelUnconfirmed->setText(RavenUnits::formatWithUnit(unit, unconfirmedBalance, false, RavenUnits::separatorAlways));
    ui->labelImmature->setText(RavenUnits::formatWithUnit(unit, immatureBalance, false, RavenUnits::separatorAlways));
    ui->labelTotal->setText(RavenUnits::formatWithUnit(unit, balance + unconfirmedBalance + immatureBalance, false, RavenUnits::separatorAlways));
    ui->labelWatchAvailable->setText(RavenUnits::formatWithUnit(unit, watchOnlyBalance, false, RavenUnits::separatorAlways));
    ui->labelWatchPending->setText(RavenUnits::formatWithUnit(unit, watchUnconfBalance, false, RavenUnits::separatorAlways));
    ui->labelWatchImmature->setText(RavenUnits::formatWithUnit(unit, watchImmatureBalance, false, RavenUnits::separatorAlways));
    ui->labelWatchTotal->setText(RavenUnits::formatWithUnit(unit, watchOnlyBalance + watchUnconfBalance + watchImmatureBalance, false, RavenUnits::separatorAlways));

    ui->labelImmature->setVisible(true);
    ui->labelImmatureText->setVisible(true);
    ui->labelWatchImmature->setVisible(watchImmatureBalance != 0);
    updateUsdt();
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    ui->labelSpendable->setVisible(showWatchOnly);      // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(showWatchOnly);      // show watch-only label
    ui->lineWatchBalance->setVisible(showWatchOnly);    // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    ui->labelWatchPending->setVisible(showWatchOnly);   // show watch-only pending balance
    ui->labelWatchTotal->setVisible(showWatchOnly);     // show watch-only total balance

    if (!showWatchOnly)
        ui->labelWatchImmature->hide();
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
        connect(model, SIGNAL(lotteryChanged()), this, SLOT(updateLottery()));
        updateLottery();
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        assetFilter.reset(new AssetFilterProxy());
        assetFilter->setSourceModel(model->getAssetTableModel());
        assetFilter->setDynamicSortFilter(true);
        assetFilter->setSortRole(AssetTableModel::AssetNameRole);
        assetFilter->sort(AssetTableModel::Name, Qt::AscendingOrder);
        ui->listAssets->setModel(assetFilter.get());
        ui->listAssets->setAutoFillBackground(false);
        connect(assetFilter.get(), SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(updateAssetEmpty()));
        connect(assetFilter.get(), SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(updateAssetEmpty()));
        connect(assetFilter.get(), SIGNAL(modelReset()), this, SLOT(updateAssetEmpty()));
        connect(assetFilter.get(), SIGNAL(layoutChanged()), this, SLOT(updateAssetEmpty()));
        updateAssetEmpty();

        ui->assetVerticalSpaceWidget->setStyleSheet("background-color: transparent");
        ui->assetVerticalSpaceWidget2->setStyleSheet("background-color: transparent");


        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(),
                   model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        connect(model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)), this, SLOT(syncCoinControl()));
        syncCoinControl();

        updateWatchOnlyLabels(model->haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    // update the display unit, to not use the default ("RVN")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance,
                       currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
    if (AreAssetsDeployed()) {
        ui->labelAssetStatus->setVisible(fShow);
    }
}

void OverviewPage::showAssets()
{
    if (AreAssetsDeployed()) {
        ui->assetFrame->show();
        ui->assetBalanceLabel->show();
        ui->labelAssetStatus->show();

        // Disable the vertical space so that listAssets goes to the bottom of the screen
        ui->assetVerticalSpaceWidget->hide();
        ui->assetVerticalSpaceWidget2->hide();
    } else {
        ui->assetFrame->hide();
        ui->assetBalanceLabel->hide();
        ui->labelAssetStatus->hide();

        // This keeps the RVN balance grid from expanding and looking terrible when asset balance is hidden
        ui->assetVerticalSpaceWidget->show();
        ui->assetVerticalSpaceWidget2->show();
    }
}

void OverviewPage::updateLottery()
{
    if (!clientModel) {
        lotteryStatus->setText(tr("Node not ready"));
        lotteryDetail->setText(QString());
        return;
    }
    ClientModel::LotteryGuiInfo info = clientModel->getLotteryGuiInfo();
    QString handle = info.handle.isEmpty() ? tr("(no handle)") : info.handle;
    if (info.eligible) {
        lotteryStatus->setText(tr("Eligible · %1").arg(handle));
    } else {
        lotteryStatus->setText(tr("Not eligible · %1").arg(handle));
    }
    QString win = info.isWinner ? tr("this slot: winner") : tr("watching");
    lotteryDetail->setText(tr("Height %1 · slot %2 · %3 active · %4 winner(s) · %5")
                               .arg(info.height)
                               .arg(info.slot)
                               .arg(info.activeNodes)
                               .arg(info.winnerCount)
                               .arg(win));
}

void OverviewPage::assetSearchChanged()
{
    if (!assetFilter)
        return;
    assetFilter->setAssetNameContains(ui->assetSearch->text());
    updateAssetEmpty();
}

void OverviewPage::openIPFSForAsset(const QModelIndex &index)
{
    // Get the ipfs hash of the asset clicked
    QString ipfshash = index.data(AssetTableModel::AssetIPFSHashRole).toString();
    QString ipfsbrowser = walletModel->getOptionsModel()->getIpfsUrl();

    // If the ipfs hash isn't there or doesn't start with Qm, disable the action item
    if (ipfshash.count() > 0 && ipfshash.indexOf("Qm") == 0 && ipfsbrowser.indexOf("http") == 0)
    {
        QUrl ipfsurl = QUrl::fromUserInput(ipfsbrowser.replace("%s", ipfshash));

        // Create the box with everything.
        if(QMessageBox::Yes == QMessageBox::question(this,
                                                        tr("Open IPFS content?"),
                                                        tr("Open the following IPFS content in your default browser?\n")
                                                        + ipfsurl.toString()
                                                    ))
        {
        QDesktopServices::openUrl(ipfsurl);
        }
    }
}

void OverviewPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    syncCoinControl();
    updateUsdt();
    updateAssetEmpty();
}

void OverviewPage::applyChrome()
{
    const bool dark = darkModeEnabled;
    const QString page = dark ? QStringLiteral("#000000") : QStringLiteral("#f4f4f5");
    const QString card = dark ? QStringLiteral("#111111") : QStringLiteral("#ffffff");
    const QString border = dark ? QStringLiteral("#2a2a2a") : QStringLiteral("#e4e4e7");
    const QString text = dark ? QStringLiteral("#fafafa") : QStringLiteral("#18181b");
    const QString muted = dark ? QStringLiteral("#a1a1aa") : QStringLiteral("#71717a");
    const QString field = dark ? QStringLiteral("#0a0a0a") : QStringLiteral("#ffffff");
    const QString handle = dark ? QStringLiteral("#3f3f46") : QStringLiteral("#d4d4d8");
    const QString btnBg = dark ? QStringLiteral("#fafafa") : QStringLiteral("#18181b");
    const QString btnFg = dark ? QStringLiteral("#18181b") : QStringLiteral("#fafafa");

    setStyleSheet(QStringLiteral("QWidget#OverviewPage { background-color: %1; }").arg(page));

    auto cardCss = [&](const char* name) {
        return QStringLiteral(
            "QFrame#%1 { background-color: %2; border: 1px solid %3; border-radius: 14px; }")
            .arg(QLatin1String(name), card, border);
    };

    ui->frame->setGraphicsEffect(nullptr);
    ui->frame_2->setGraphicsEffect(nullptr);
    ui->assetFrame->setGraphicsEffect(nullptr);
    ui->frame->setAutoFillBackground(false);
    ui->frame_2->setAutoFillBackground(false);
    ui->assetFrame->setAutoFillBackground(false);
    ui->frame->setStyleSheet(cardCss("frame"));
    ui->frame_2->setStyleSheet(cardCss("frame_2"));
    ui->assetFrame->setStyleSheet(cardCss("assetFrame"));
    ui->frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ui->assetFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    ui->frame_2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    ui->horizontalLayout->setSpacing(16);
    ui->horizontalLayout->setContentsMargins(24, 20, 24, 20);
    ui->verticalLayout_2->setSpacing(14);
    ui->verticalLayout_4->setContentsMargins(18, 16, 18, 16);
    ui->verticalLayout_4->setSpacing(8);
    ui->verticalLayout_5->setContentsMargins(18, 16, 14, 16);
    ui->verticalLayout_5->setSpacing(10);
    ui->gridLayout_2->setContentsMargins(18, 16, 12, 16);

    ui->line->hide();
    ui->line_2->hide();
    ui->assetBalanceLine->hide();
    ui->line_3->hide();

    const QString strong = QStringLiteral("color: %1; background: transparent; border: none;").arg(text);
    const QString quiet = QStringLiteral("color: %1; background: transparent; border: none;").arg(muted);
    ui->rvnBalancesLabel->setStyleSheet(strong);
    ui->assetBalanceLabel->setStyleSheet(strong);
    ui->recentTransactionsLabel->setStyleSheet(strong);
    ui->labelTotalText->setStyleSheet(strong);
    ui->labelTotal->setStyleSheet(strong);
    ui->labelWatchTotal->setStyleSheet(strong);
    ui->labelBalance->setStyleSheet(strong);
    ui->labelUnconfirmed->setStyleSheet(strong);
    ui->labelImmature->setStyleSheet(strong);
    ui->labelWatchAvailable->setStyleSheet(strong);
    ui->labelWatchPending->setStyleSheet(strong);
    ui->labelWatchImmature->setStyleSheet(strong);
    ui->labelBalanceText->setStyleSheet(quiet);
    ui->labelPendingText->setStyleSheet(quiet);
    ui->labelImmatureText->setStyleSheet(quiet);
    ui->labelSpendable->setStyleSheet(quiet);
    ui->labelWatchonly->setStyleSheet(quiet);
    if (usdtLabel)
        usdtLabel->setStyleSheet(quiet);
    if (assetEmpty)
        assetEmpty->setStyleSheet(quiet);

    QFont section = ui->rvnBalancesLabel->font();
    section.setPixelSize(15);
    section.setWeight(QFont::DemiBold);
    ui->rvnBalancesLabel->setFont(section);
    ui->assetBalanceLabel->setFont(section);
    ui->recentTransactionsLabel->setFont(section);

    QFont caption = ui->labelBalanceText->font();
    caption.setPixelSize(13);
    ui->labelBalanceText->setFont(caption);
    ui->labelPendingText->setFont(caption);
    ui->labelImmatureText->setFont(caption);
    ui->labelSpendable->setFont(caption);
    ui->labelWatchonly->setFont(caption);
    ui->labelTotalText->setFont(caption);

    QFont amount = ui->labelBalance->font();
    amount.setPixelSize(14);
    ui->labelBalance->setFont(amount);
    ui->labelUnconfirmed->setFont(amount);
    ui->labelImmature->setFont(amount);
    ui->labelWatchAvailable->setFont(amount);
    ui->labelWatchPending->setFont(amount);
    ui->labelWatchImmature->setFont(amount);

    QFont totalFont = ui->labelTotal->font();
    totalFont.setPixelSize(18);
    totalFont.setWeight(QFont::DemiBold);
    ui->labelTotal->setFont(totalFont);
    ui->labelWatchTotal->setFont(totalFont);

    if (usdtLabel) {
        QFont usdtFont = caption;
        usdtFont.setPixelSize(13);
        usdtLabel->setFont(usdtFont);
    }

    QFrame* lotteryFrame = findChild<QFrame*>(QStringLiteral("lotteryFrame"));
    if (lotteryFrame) {
        lotteryFrame->setGraphicsEffect(nullptr);
        lotteryFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        lotteryFrame->setStyleSheet(QStringLiteral(
            "QFrame#lotteryFrame { background-color: transparent; border: none; }"));
    }
    if (lotteryTitle) {
        QFont lotteryFont = caption;
        lotteryFont.setPixelSize(12);
        lotteryFont.setWeight(QFont::Medium);
        lotteryTitle->setFont(lotteryFont);
        lotteryTitle->setStyleSheet(quiet);
    }
    if (lotteryStatus) {
        lotteryStatus->setFont(caption);
        lotteryStatus->setStyleSheet(strong);
    }
    if (lotteryDetail) {
        QFont detail = caption;
        detail.setPixelSize(12);
        lotteryDetail->setFont(detail);
        lotteryDetail->setStyleSheet(quiet);
    }

    const QString listCss = QStringLiteral(
        "QListView { background: transparent; border: none; color: %1; outline: none; }"
        "QListView::item { background: transparent; border: none; }"
        "QListView::item:selected { background: transparent; color: %1; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 4px 0; border: none; }"
        "QScrollBar::handle:vertical { background: %2; min-height: 28px; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; background: transparent; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
        .arg(text, handle);
    ui->listAssets->setStyleSheet(listCss);
    ui->listTransactions->setStyleSheet(listCss);

    const QString editCss = QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 8px; padding: 8px 10px; }"
        "QLineEdit:focus { border: 1px solid %4; }")
        .arg(field, text, border, muted);
    ui->assetSearch->setStyleSheet(editCss);
    ui->assetSearch->setMinimumHeight(36);
    if (sendDest)
        sendDest->setStyleSheet(editCss);
    if (sendAmount)
        sendAmount->setStyleSheet(editCss);

    QFrame* sendCard = findChild<QFrame*>(QStringLiteral("balanceSendCard"));
    if (sendCard) {
        sendCard->setStyleSheet(QStringLiteral(
            "QFrame#balanceSendCard { background-color: %1; border: 1px solid %2; border-radius: 14px; }")
            .arg(card, border));
        sendCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    }
    if (QLabel* sendTitle = findChild<QLabel*>(QStringLiteral("balanceSendTitle"))) {
        sendTitle->setFont(section);
        sendTitle->setStyleSheet(strong);
    }
    if (coinControlCheck) {
        coinControlCheck->setStyleSheet(QStringLiteral(
            "QCheckBox { color: %1; background: transparent; spacing: 8px; }"
            "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid %2; border-radius: 4px; background: %3; }"
            "QCheckBox::indicator:checked { background: %1; border-color: %1; }")
            .arg(text, border, field));
    }
    if (chooseCoinsBtn) {
        chooseCoinsBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 8px; padding: 8px 12px; }"
            "QPushButton:hover { border-color: %1; }")
            .arg(text, border));
    }
    if (QPushButton* sendBtn = findChild<QPushButton*>(QStringLiteral("balanceSendBtn"))) {
        sendBtn->setStyleSheet(QStringLiteral(
            "QPushButton#balanceSendBtn { background: %1; color: %2; border: none; border-radius: 8px; padding: 10px 14px; }"
            "QPushButton#balanceSendBtn:hover { background: %3; }")
            .arg(btnBg, btnFg, muted));
    }
    if (coinSummary)
        coinSummary->setStyleSheet(quiet);
    if (sendStatus)
        sendStatus->setStyleSheet(quiet);
}

void OverviewPage::buildSendCard()
{
    QFrame* card = new QFrame(this);
    card->setObjectName(QStringLiteral("balanceSendCard"));
    QVBoxLayout* lay = new QVBoxLayout(card);
    lay->setContentsMargins(18, 16, 18, 16);
    lay->setSpacing(8);

    QLabel* title = new QLabel(tr("Send XFER"), card);
    title->setObjectName(QStringLiteral("balanceSendTitle"));
    lay->addWidget(title);

    sendDest = new QLineEdit(card);
    sendDest->setPlaceholderText(tr("@handle or X address"));
    lay->addWidget(sendDest);

    sendAmount = new QLineEdit(card);
    sendAmount->setPlaceholderText(tr("Amount (XFER)"));
    lay->addWidget(sendAmount);

    coinControlCheck = new QCheckBox(tr("Coin control"), card);
    coinControlCheck->setCursor(Qt::PointingHandCursor);
    lay->addWidget(coinControlCheck);

    chooseCoinsBtn = new QPushButton(tr("Choose coins"), card);
    chooseCoinsBtn->setCursor(Qt::PointingHandCursor);
    chooseCoinsBtn->setVisible(false);
    lay->addWidget(chooseCoinsBtn);

    coinSummary = new QLabel(card);
    coinSummary->setWordWrap(true);
    coinSummary->setVisible(false);
    lay->addWidget(coinSummary);

    QPushButton* send = new QPushButton(tr("Send"), card);
    send->setObjectName(QStringLiteral("balanceSendBtn"));
    send->setCursor(Qt::PointingHandCursor);
    send->setMinimumHeight(40);
    lay->addWidget(send);

    sendStatus = new QLabel(card);
    sendStatus->setWordWrap(true);
    lay->addWidget(sendStatus);

    connect(send, SIGNAL(clicked()), this, SLOT(onBalanceSend()));
    connect(coinControlCheck, SIGNAL(toggled(bool)), this, SLOT(onCoinControlToggled(bool)));
    connect(chooseCoinsBtn, SIGNAL(clicked()), this, SLOT(onChooseCoins()));

    ui->verticalLayout_2->insertWidget(2, card);
}

void OverviewPage::clearSendForm()
{
    if (sendDest)
        sendDest->clear();
    if (sendAmount)
        sendAmount->clear();
    if (sendStatus)
        sendStatus->clear();
    syncCoinControl();
}

void OverviewPage::updateUsdt()
{
    if (!usdtLabel)
        return;
    if (!priceClient || !priceClient->hasPrice()
        || currentBalance < 0 || currentUnconfirmedBalance < 0 || currentImmatureBalance < 0) {
        usdtLabel->hide();
        return;
    }
    const double px = priceClient->lastPrice();
    const CAmount total = currentBalance + currentUnconfirmedBalance + currentImmatureBalance;
    const double xfer = static_cast<double>(total) / static_cast<double>(COIN);
    const double usdt = xfer * px;
    usdtLabel->setText(tr("XFER/USDT  %1\nWallet total  %2 USDT")
        .arg(QString::fromStdString(xprice::FormatPrice(px)))
        .arg(QString::fromStdString(xprice::FormatUsdt(usdt))));
    usdtLabel->show();
}

void OverviewPage::updateAssetEmpty()
{
    if (!assetEmpty)
        return;
    if (!assetFilter) {
        assetEmpty->hide();
        return;
    }
    const bool filtering = ui->assetSearch && !ui->assetSearch->text().trimmed().isEmpty();
    if (assetFilter->rowCount() > 0) {
        assetEmpty->hide();
        ui->listAssets->show();
        return;
    }
    ui->listAssets->hide();
    assetEmpty->setText(filtering
        ? tr("No assets match that search.")
        : tr("No assets in this wallet yet."));
    assetEmpty->show();
}

void OverviewPage::syncCoinControl()
{
    const bool on = xcoinsend::CoinControlEnabled(walletModel);
    if (coinControlCheck) {
        coinControlCheck->blockSignals(true);
        coinControlCheck->setChecked(on);
        coinControlCheck->blockSignals(false);
    }
    if (chooseCoinsBtn)
        chooseCoinsBtn->setVisible(on);
    if (coinSummary) {
        coinSummary->setVisible(on);
        coinSummary->setText(xcoinsend::CoinSelectionText(on));
    }
}

void OverviewPage::onCoinControlToggled(bool checked)
{
    if (!walletModel || !walletModel->getOptionsModel()) {
        syncCoinControl();
        return;
    }
    xcoinsend::SetCoinControlEnabled(walletModel->getOptionsModel(), checked);
    syncCoinControl();
    if (checked)
        onChooseCoins();
}

void OverviewPage::onChooseCoins()
{
    xcoinsend::ChooseCoins(this, balanceStyle, walletModel);
    syncCoinControl();
}

void OverviewPage::onBalanceSend()
{
    if (!sendDest || !sendAmount)
        return;
    const QString dest = sendDest->text().trimmed();
    const QString amt = sendAmount->text().trimmed();
    if (dest.isEmpty() || amt.isEmpty()) {
        QMessageBox::information(this, tr("X Coin"), tr("Enter a name or address, and an amount."));
        return;
    }
    QString displayName;
    QString resolveErr;
    const QString resolved = xcoinsend::ResolveDestination(dest, &displayName, &resolveErr);
    if (resolved.isEmpty()) {
        const QString msg = resolveErr.isEmpty() ? tr("Could not resolve that name.") : resolveErr;
        if (sendStatus)
            sendStatus->setText(msg);
        QMessageBox::warning(this, tr("X Coin"), msg);
        return;
    }
    const bool coinControl = xcoinsend::CoinControlEnabled(walletModel);
    const xcoinsend::Outcome outcome = xcoinsend::SendResolved(this, walletModel, resolved, displayName, amt, coinControl);
    if (outcome.result == xcoinsend::Cancelled)
        return;
    if (outcome.result == xcoinsend::Sent) {
        WalletView* view = qobject_cast<WalletView*>(parentWidget());
        if (view)
            view->clearSendDrafts();
        else
            clearSendForm();
        if (sendStatus) {
            sendStatus->setText(outcome.message.isEmpty()
                ? tr("Sent.")
                : tr("Sent.\n%1").arg(outcome.message));
        }
        return;
    }
    const QString msg = outcome.message.isEmpty() ? tr("Send failed.") : outcome.message;
    if (sendStatus)
        sendStatus->setText(msg);
    QMessageBox::warning(this, tr("X Coin"), msg);
}
