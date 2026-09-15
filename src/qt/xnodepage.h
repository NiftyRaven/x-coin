// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XNODEPAGE_H
#define RAVEN_QT_XNODEPAGE_H

#include <QWidget>
#include <QString>

class ClientModel;
class WalletView;

class QCheckBox;
class QLabel;
class QLineEdit;

class XNodePage : public QWidget
{
    Q_OBJECT

public:
    explicit XNodePage(WalletView* walletView, QWidget* parent = 0);
    void setClientModel(ClientModel* clientModel);

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void onShareToggled(bool on);
    void onCopy();

private:
    void applyTheme();
    QString rpc(const QString& method, const QStringList& args = QStringList()) const;
    QString localListenEndpoint() const;
    void fillShareWidgets();
    void applyShareVisibility(bool on);

    WalletView* walletView;
    ClientModel* clientModel;
    QLabel* statusCard;
    QLabel* endpointLabel;
    QLabel* hintLabel;
    QLineEdit* nodeIpEdit;
    QCheckBox* shareChk;
    QWidget* sharePanel;
};

#endif
