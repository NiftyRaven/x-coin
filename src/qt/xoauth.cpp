// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xoauth.h"

#include "crypto/sha256.h"
#include "random.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "xsession.h"

#include <QDesktopServices>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QAbstractSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <fstream>

static std::string Base64Url(const unsigned char* p, size_t n)
{
    std::string s = EncodeBase64(p, n);
    for (char& c : s) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!s.empty() && s.back() == '=')
        s.pop_back();
    return s;
}

static QString RandomB64Url(int nbytes)
{
    std::vector<unsigned char> buf((size_t)nbytes);
    GetRandBytes(buf.data(), nbytes);
    return QString::fromStdString(Base64Url(buf.data(), buf.size()));
}

XOAuth::XOAuth(QObject *parent) :
    QObject(parent),
    server(new QTcpServer(this)),
    nam(new QNetworkAccessManager(this)),
    callbackConsumed(false)
{
    connect(server, SIGNAL(newConnection()), this, SLOT(onIncoming()));
}

XOAuth::~XOAuth()
{
    wipeSecrets();
    if (server->isListening())
        server->close();
}

QString XOAuth::ClientId()
{
    return QString::fromStdString(gArgs.GetArg("-xoauthclientid", "")).trimmed();
}

int XOAuth::CallbackPort()
{
    const int p = (int)gArgs.GetArg("-xoauthcallbackport", 18791);
    if (p < 1 || p > 65535)
        return 18791;
    return p;
}

QString XOAuth::CallbackUri()
{
    return QString("http://127.0.0.1:%1/callback").arg(CallbackPort());
}

bool XOAuth::SaveClientId(const QString& clientId, QString& err)
{
    const std::string id = clientId.trimmed().toStdString();
    if (id.empty()) {
        err = "Client ID is empty";
        return false;
    }
    gArgs.ForceSetArg("-xoauthclientid", id);
    const fs::path conf = GetConfigFile(gArgs.GetArg("-conf", "xcoin.conf"));
    std::string existing;
    {
        std::ifstream in(conf.string().c_str());
        std::string line;
        bool replaced = false;
        std::string out;
        while (std::getline(in, line)) {
            std::string t = line;
            while (!t.empty() && (t[0] == ' ' || t[0] == '\t'))
                t.erase(t.begin());
            if (t.compare(0, 15, "xoauthclientid=") == 0 || t.compare(0, 16, "xoauthclientid =") == 0) {
                out += "xoauthclientid=" + id + "\n";
                replaced = true;
            } else {
                out += line + "\n";
            }
        }
        if (!replaced)
            out += "xoauthclientid=" + id + "\n";
        existing = out;
    }
    std::ofstream out(conf.string().c_str(), std::ios::trunc);
    if (!out) {
        err = QString("cannot write %1").arg(QString::fromStdString(conf.string()));
        return false;
    }
    out << existing;
    return true;
}

void XOAuth::fail(const QString& e)
{
    wipeSecrets();
    Q_EMIT failed(e);
}

void XOAuth::wipeSecrets()
{
    verifier.fill(QChar('0'));
    verifier.clear();
    state.fill(QChar('0'));
    state.clear();
    pendingCode.fill(QChar('0'));
    pendingCode.clear();
    accessToken.fill(QChar('0'));
    accessToken.clear();
}

void XOAuth::startLogin()
{
    const QString client = ClientId();
    if (client.isEmpty()) {
        fail(tr("The operator has not baked an X app Client ID. Sign in cannot continue. "
                "There is no Client ID field here. The operator puts xoauthclientid= in the "
                "xcoin.conf shipped in the Windows and Linux wallet folders "
                "(callback http://127.0.0.1:18791/callback)."));
        return;
    }
    verifier = RandomB64Url(32);
    state = RandomB64Url(16);
    unsigned char hash[CSHA256::OUTPUT_SIZE];
    const std::string ver = verifier.toStdString();
    CSHA256().Write((const unsigned char*)ver.data(), ver.size()).Finalize(hash);
    const QString challenge = QString::fromStdString(Base64Url(hash, sizeof(hash)));

    callbackConsumed = false;
    if (server->isListening())
        server->close();
    if (!server->listen(QHostAddress::LocalHost, (quint16)CallbackPort())) {
        fail(tr("Cannot bind http://127.0.0.1:%1/callback — pick another -xoauthcallbackport=").arg(CallbackPort()));
        return;
    }

    QUrl url("https://twitter.com/i/oauth2/authorize");
    QUrlQuery q;
    q.addQueryItem("response_type", "code");
    q.addQueryItem("client_id", client);
    q.addQueryItem("redirect_uri", CallbackUri());
    q.addQueryItem("scope", "users.read tweet.read");
    q.addQueryItem("state", state);
    q.addQueryItem("code_challenge", challenge);
    q.addQueryItem("code_challenge_method", "S256");
    url.setQuery(q);
    Q_EMIT status(tr("Opening Sign in with X in your browser…"));
    if (!QDesktopServices::openUrl(url))
        fail(tr("Could not open a browser. Open this URL:\n%1").arg(url.toString()));
}

void XOAuth::onIncoming()
{
    QTcpSocket *sock = server->nextPendingConnection();
    if (!sock)
        return;
    sock->setParent(this);
    connect(sock, SIGNAL(readyRead()), this, SLOT(onCallbackReadyRead()));
    QTimer *timeout = new QTimer(sock);
    timeout->setSingleShot(true);
    connect(timeout, SIGNAL(timeout()), this, SLOT(onCallbackTimeout()));
    timeout->start(15000);
    if (sock->bytesAvailable() > 0)
        processCallbackSocket(sock);
}

void XOAuth::onCallbackReadyRead()
{
    processCallbackSocket(qobject_cast<QTcpSocket*>(sender()));
}

void XOAuth::onCallbackTimeout()
{
    QTimer *timeout = qobject_cast<QTimer*>(sender());
    QTcpSocket *sock = timeout ? qobject_cast<QTcpSocket*>(timeout->parent()) : 0;
    if (!sock || sock->property("xoauthDone").toBool())
        return;
    sock->setProperty("xoauthDone", true);
    sock->disconnectFromHost();
    sock->deleteLater();
    fail(tr("Sign in with X timed out waiting for the browser callback."));
}

void XOAuth::processCallbackSocket(QTcpSocket *sock)
{
    if (!sock || sock->property("xoauthDone").toBool())
        return;
    QByteArray acc = sock->property("xoauthBuf").toByteArray();
    acc += sock->readAll();
    sock->setProperty("xoauthBuf", acc);
    if (!acc.contains("\r\n\r\n") && acc.size() < 16384)
        return;

    sock->setProperty("xoauthDone", true);
    const QList<QTimer*> timers = sock->findChildren<QTimer*>();
    for (int i = 0; i < timers.size(); ++i)
        timers.at(i)->stop();

    const QString first = QString::fromUtf8(acc).split("\r\n").value(0);
    QString path = first.section(' ', 1, 1);
    const QString htmlOk =
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
        "<html><body style=\"background:#000;color:#fff;font-family:sans-serif;padding:40px\">"
        "<h1>X Coin</h1><p>Signed in. You can close this tab and return to the wallet.</p></body></html>";
    const QString htmlErr =
        "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
        "X Coin OAuth callback error\n";
    const QString htmlSkip =
        "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
        "not found\n";

    auto closeSock = [sock]() {
        sock->flush();
        QObject::connect(sock, SIGNAL(disconnected()), sock, SLOT(deleteLater()));
        sock->disconnectFromHost();
        if (sock->state() == QAbstractSocket::UnconnectedState)
            sock->deleteLater();
    };

    if (acc.size() >= 16384 && !acc.contains("\r\n\r\n")) {
        sock->write(htmlErr.toUtf8());
        closeSock();
        return;
    }

    if (!path.startsWith("/callback")) {
        sock->write(htmlSkip.toUtf8());
        closeSock();
        return;
    }

    QUrl u("http://127.0.0.1" + path);
    QUrlQuery q(u);
    const QString st = q.queryItemValue("state");
    const QString code = q.queryItemValue("code");
    const QString oauthErr = q.queryItemValue("error");
    if (!oauthErr.isEmpty() || code.isEmpty() || st != state) {
        sock->write(htmlErr.toUtf8());
        closeSock();
        fail(oauthErr.isEmpty() ? tr("Sign in with X did not complete. Try again from Home.") : oauthErr);
        return;
    }
    if (callbackConsumed) {
        sock->write(htmlOk.toUtf8());
        closeSock();
        return;
    }
    callbackConsumed = true;
    sock->write(htmlOk.toUtf8());
    closeSock();
    server->close();
    exchangeCode(code);
    pendingCode.clear();
}

void XOAuth::exchangeCode(const QString& code)
{
    Q_EMIT status(tr("Exchanging authorization code…"));
    QNetworkRequest req(QUrl("https://api.twitter.com/2/oauth2/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QUrlQuery body;
    body.addQueryItem("code", code);
    body.addQueryItem("grant_type", "authorization_code");
    body.addQueryItem("client_id", ClientId());
    body.addQueryItem("redirect_uri", CallbackUri());
    body.addQueryItem("code_verifier", verifier);
    const std::string secret = gArgs.GetArg("-xoauthclientsecret", "");
    if (!secret.empty())
        body.addQueryItem("client_secret", QString::fromStdString(secret));
    QNetworkReply *reply = nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, SIGNAL(finished()), this, SLOT(onTokenFinished()));
}

void XOAuth::onTokenFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        fail(tr("Token request failed: %1").arg(reply->errorString()));
        return;
    }
    const QByteArray raw = reply->readAll();
    // Minimal JSON extract of access_token
    const QString s = QString::fromUtf8(raw);
    const int i = s.indexOf("\"access_token\"");
    if (i < 0) {
        fail(tr("Token response had no access_token. Sign-in did not succeed."));
        return;
    }
    int colon = s.indexOf(':', i);
    int q1 = s.indexOf('"', colon + 1);
    int q2 = s.indexOf('"', q1 + 1);
    if (q1 < 0 || q2 < 0) {
        fail(tr("Could not parse access_token"));
        return;
    }
    accessToken = s.mid(q1 + 1, q2 - q1 - 1);
    if (accessToken.isEmpty()) {
        fail(tr("Empty access token — login did not succeed"));
        return;
    }
    const QString token = accessToken;
    fetchMe(token);
    // Access token is RAM-only. Never write it to disk or logs.
    wipeSecrets();
    accessToken.clear();
}

void XOAuth::fetchMe(const QString& token)
{
    Q_EMIT status(tr("Calling GET /2/users/me?user.fields=verified,verified_type…"));
    QUrl url("https://api.twitter.com/2/users/me");
    QUrlQuery q;
    q.addQueryItem("user.fields", "verified,verified_type");
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    QNetworkReply *reply = nam->get(req);
    connect(reply, SIGNAL(finished()), this, SLOT(onMeFinished()));
}

void XOAuth::onMeFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        fail(tr("users/me failed: %1").arg(reply->errorString()));
        return;
    }
    QString err;
    if (!finishFromUsersMe(reply->readAll(), err))
        fail(err);
}

bool XOAuth::finishFromUsersMe(const QByteArray& body, QString& err)
{
    xsession::UsersMe me;
    std::string e;
    if (!xsession::ParseUsersMe(std::string(body.constData(), (size_t)body.size()), me, e)) {
        err = QString::fromStdString(e);
        return false;
    }
    const int64_t exp = GetTime() + 7200;
    if (!xsession::SaveSession(me.userId, me.username, exp, e, me.verified, me.verifiedType)) {
        err = QString::fromStdString(e);
        return false;
    }
    xsession::BindLotteryFromSession();
    wipeSecrets();
    Q_EMIT signedIn(QString::fromStdString(me.username), QString());
    return true;
}

bool XOAuth::mockSignIn(const QString& payload, QString& err)
{
    std::string e;
    if (!xsession::ApplyUsersMePayload(payload.toStdString(), e, nullptr)) {
        err = QString::fromStdString(e);
        return false;
    }
    Q_EMIT signedIn(QString::fromStdString(xsession::SignedInHandle()),
                    QString());
    return true;
}
