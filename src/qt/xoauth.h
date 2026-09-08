// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XOAUTH_H
#define RAVEN_QT_XOAUTH_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTcpServer;
class QTcpSocket;

/** OAuth 2.0 PKCE "Sign in with X". Loopback http://127.0.0.1:<port>/callback */
class XOAuth : public QObject
{
    Q_OBJECT
public:
    explicit XOAuth(QObject *parent = 0);
    ~XOAuth();

    static QString ClientId();
    static bool SaveClientId(const QString& clientId, QString& err);
    static int CallbackPort();
    static QString CallbackUri();

    void startLogin();
    /** Regtest only: inject a mock users/me payload. */
    bool mockSignIn(const QString& payload, QString& err);

Q_SIGNALS:
    void signedIn(QString username, QString userId);
    void failed(QString error);
    void status(QString message);

private Q_SLOTS:
    void onIncoming();
    void onCallbackReadyRead();
    void onCallbackTimeout();
    void onTokenFinished();
    void onMeFinished();

private:
    void fail(const QString& e);
    void wipeSecrets();
    void exchangeCode(const QString& code);
    void fetchMe(const QString& accessToken);
    bool finishFromUsersMe(const QByteArray& body, QString& err);
    void processCallbackSocket(QTcpSocket *sock);

    QTcpServer *server;
    QNetworkAccessManager *nam;
    QString verifier;
    QString state;
    QString pendingCode;
    QString accessToken;
    bool callbackConsumed;
};

#endif
