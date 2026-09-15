// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XSWAP_H
#define RAVEN_QT_XSWAP_H

#include <QWidget>
#include <QString>

class WalletModel;
class WalletView;

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QWidget;

/** Asset marketplace (P2P asks) plus optional private OTC. */
class XSwap : public QWidget
{
    Q_OBJECT

public:
    explicit XSwap(WalletView* walletView, QWidget* parent = 0);
    void setWalletModel(WalletModel* walletModel);

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void onPollBook();
    void onBuy();
    void onList();
    void onCancel();
    void onToggleOtc();
    void onMakeOffer();
    void onAcceptOffer();
    void onFinish();
    void onCopyOffer();
    void onCopySigned();

private:
    void applyTheme();
    QString rpc(const QString& method, const QStringList& args = QStringList()) const;
    bool rpcOk(const QString& method, const QStringList& args, QString* out, QString* err) const;
    void showStatus(const QString& text, bool err, bool popup = true);
    void fillAssetCombos();
    void fillHoldings();
    void fillBook();
    QString selectedId(QTableWidget* table) const;
    QString newLabeledAddress(const QString& label);
    QString comboAsset(QComboBox* box) const;

    WalletView* walletView;
    WalletModel* walletModel;
    QTableWidget* bookTable;
    QTableWidget* mineTable;
    QTableWidget* holdTable;
    QLineEdit* listPrice;
    QLabel* bookEmpty;
    QLabel* statusLabel;
    QPushButton* otcToggle;
    QWidget* otcWidget;
    QComboBox* giveAsset;
    QComboBox* wantAsset;
    QLineEdit* giveAmt;
    QLineEdit* wantAmt;
    QLineEdit* feeEdit;
    QPlainTextEdit* offerBox;
    QPlainTextEdit* signedBox;
    QLabel* previewLabel;
};

#endif
