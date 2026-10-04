// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xpriceclient.h"

#include "xprice.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

XPriceClient::XPriceClient(QObject* parent)
    : QObject(parent)
    , nam(new QNetworkAccessManager(this))
    , timer(new QTimer(this))
    , have(false)
    , price(0.0)
{
    timer->setInterval(60 * 1000);
    connect(nam, &QNetworkAccessManager::finished, this, [this](QNetworkReply* reply) { handle(reply); });
    connect(timer, &QTimer::timeout, this, [this]() { refresh(); });
    timer->start();
    refresh();
}

void XPriceClient::refresh()
{
    QNetworkRequest req(QUrl(QString::fromLatin1(xprice::TickerUrl())));
    req.setRawHeader("User-Agent", "XCoinWallet");
    req.setRawHeader("Accept", "application/json");
    nam->get(req);
}

void XPriceClient::handle(QNetworkReply* reply)
{
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
        return;
    const QByteArray body = reply->readAll();
    double next = 0.0;
    std::string err;
    if (!xprice::ParseLastPrice(body.toStdString(), next, err))
        return;
    price = next;
    have = true;
    if (onUpdate)
        onUpdate();
}
