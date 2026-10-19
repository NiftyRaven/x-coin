// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XOAUTH_H
#define RAVEN_QT_XOAUTH_H

#include "xoauth_token.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

class QNetworkAccessManager;
class QNetworkReply;
class QTcpServer;
class QTcpSocket;
class QTimer;

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
    /** True while the loopback listener is still bound for this login. */
    bool loginPending() const;
    /** Regtest only: inject a mock users/me payload. */
    bool mockSignIn(const QString& payload, QString& err);

    /** Local retry wait (1 minute) after a failed or abandoned Sign in. Success does not stamp it. */
    static qint64 CooldownMs();
    static qint64 LoginWaitMs();
    static qint64 cooldownRemainingMs();
    /** Last successful OAuth users/me (never invented). Empty / false if none. */
    static bool cachedLinked();
    static QString cachedHandle();
    static bool cachedVerified();

Q_SIGNALS:
    void signedIn(QString username, QString userId);
    void failed(QString error);
    void status(QString message);

private Q_SLOTS:
    void onIncoming();
    void onCallbackReadyRead();
    void onCallbackTimeout();
    void onLoginWaitTimeout();
    void onTokenFinished();
    void onMeFinished();

private:
    void fail(const QString& e);
    void wipeSecrets();
    void closeListeners();
    void stopLoginWait();
    void exchangeCode(const QString& code);
    void fetchMe(const QString& token);
    bool finishFromUsersMe(const QByteArray& body, QString& err);
    void processCallbackSocket(QTcpSocket *sock);

    QTcpServer *server;
    QTcpServer *ipv6Server;
    QTimer *loginWait;
    QNetworkAccessManager *nam;
    QString verifier;
    QString state;
    QString pendingCode;
    XOAuthUsersMeHandoff tokenHold;
    bool callbackConsumed;
};

#endif
