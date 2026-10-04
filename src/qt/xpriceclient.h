// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XPRICECLIENT_H
#define RAVEN_QT_XPRICECLIENT_H

#include <QObject>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

/** Fetches the public NeoxEX XFER/USDT ticker. No API key. */
class XPriceClient : public QObject
{
public:
    explicit XPriceClient(QObject* parent = 0);

    void refresh();
    bool hasPrice() const { return have; }
    double lastPrice() const { return price; }
    void setOnUpdate(const std::function<void()>& fn) { onUpdate = fn; }

private:
    void handle(QNetworkReply* reply);

    QNetworkAccessManager* nam;
    QTimer* timer;
    bool have;
    double price;
    std::function<void()> onUpdate;
};

#endif // RAVEN_QT_XPRICECLIENT_H
