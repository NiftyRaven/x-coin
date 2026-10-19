// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xoauth.h"

#include "crypto/sha256.h"
#include "random.h"
#include "support/cleanse.h"
#include "util.h"
#include "utilstrencodings.h"
#include "xoauth_cooldown.h"
#include "xsession.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QProcess>
#include <QProcessEnvironment>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QAbstractSocket>
#include <QSettings>
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

static bool OpenSignInUrl(const QUrl& url)
{
    const QString target = url.toString();
    // The Linux launcher puts the bundled lib/ on LD_LIBRARY_PATH so xcoin-qt
    // starts. xdg-open and kde-open inherit it and die on the old libstdc++.
    // Drop that path for the browser helper only.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("LD_LIBRARY_PATH");
    env.remove("LD_PRELOAD");

    struct Helper { const char *program; bool gio; };
    const Helper helpers[] = {
        {"xdg-open", false},
        {"/usr/bin/xdg-open", false},
        {"gio", true},
        {"kde-open", false},
        {"kde-open5", false},
        {"/usr/bin/kde-open", false},
        {"firefox", false},
        {"chromium", false},
    };
    for (const Helper& h : helpers) {
        QProcess process;
        process.setProcessEnvironment(env);
        process.setProgram(QString::fromLatin1(h.program));
        if (h.gio)
            process.setArguments(QStringList() << "open" << target);
        else
            process.setArguments(QStringList() << target);
        if (process.startDetached())
            return true;
    }
    return QDesktopServices::openUrl(url);
}

static const char *kOAuthLastAttemptMs = "xoauthLastAttemptMs";
static const char *kOAuthCachedLinked = "xoauthCachedLinked";
static const char *kOAuthCachedHandle = "xoauthCachedHandle";
static const char *kOAuthCachedVerified = "xoauthCachedVerified";

static void CleanseStdString(std::string& s)
{
    if (!s.empty())
        memory_cleanse(&s[0], s.size());
    s.clear();
}

static void MarkOAuthAttempt()
{
    QSettings settings;
    settings.setValue(kOAuthLastAttemptMs, QDateTime::currentMSecsSinceEpoch());
}

static void CacheLinkedFromUsersMe(const QString& handle, bool verified)
{
    QSettings settings;
    settings.setValue(kOAuthCachedLinked, !handle.isEmpty());
    settings.setValue(kOAuthCachedHandle, handle);
    // Store only what users/me (or a regtest mock of it) reported. Never invent true.
    settings.setValue(kOAuthCachedVerified, verified);
}

XOAuth::XOAuth(QObject *parent) :
    QObject(parent),
    server(new QTcpServer(this)),
    ipv6Server(new QTcpServer(this)),
    loginWait(new QTimer(this)),
    nam(new QNetworkAccessManager(this)),
    callbackConsumed(false)
{
    connect(server, SIGNAL(newConnection()), this, SLOT(onIncoming()));
    connect(ipv6Server, SIGNAL(newConnection()), this, SLOT(onIncoming()));
    loginWait->setSingleShot(true);
    connect(loginWait, SIGNAL(timeout()), this, SLOT(onLoginWaitTimeout()));
}

XOAuth::~XOAuth()
{
    stopLoginWait();
    closeListeners();
    wipeSecrets();
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

qint64 XOAuth::CooldownMs()
{
    return XOAUTH_COOLDOWN_MS;
}

qint64 XOAuth::LoginWaitMs()
{
    return XOAUTH_LOGIN_WAIT_MS;
}

qint64 XOAuth::cooldownRemainingMs()
{
    QSettings settings;
    const qint64 last = settings.value(kOAuthLastAttemptMs, 0).toLongLong();
    return XOAuthCooldownRemainingMs(QDateTime::currentMSecsSinceEpoch(), last, XOAUTH_COOLDOWN_MS);
}

bool XOAuth::cachedLinked()
{
    QSettings settings;
    return settings.value(kOAuthCachedLinked, false).toBool()
        && !settings.value(kOAuthCachedHandle).toString().trimmed().isEmpty();
}

QString XOAuth::cachedHandle()
{
    QSettings settings;
    return settings.value(kOAuthCachedHandle).toString().trimmed();
}

bool XOAuth::cachedVerified()
{
    QSettings settings;
    // Default false. Only a stored users/me result can make this true.
    return settings.value(kOAuthCachedVerified, false).toBool();
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
    stopLoginWait();
    closeListeners();
    wipeSecrets();
    Q_EMIT failed(e);
}

void XOAuth::closeListeners()
{
    if (server && server->isListening())
        server->close();
    if (ipv6Server && ipv6Server->isListening())
        ipv6Server->close();
}

void XOAuth::stopLoginWait()
{
    if (loginWait)
        loginWait->stop();
}

bool XOAuth::loginPending() const
{
    return (server && server->isListening()) || (ipv6Server && ipv6Server->isListening());
}

void XOAuth::wipeSecrets()
{
    verifier.fill(QChar('0'));
    verifier.clear();
    state.fill(QChar('0'));
    state.clear();
    pendingCode.fill(QChar('0'));
    pendingCode.clear();
    CleanseStdString(tokenHold.held);
    CleanseStdString(tokenHold.sentToUsersMe);
}

void XOAuth::startLogin()
{
    const qint64 left = cooldownRemainingMs();
    if (left > 0) {
        const qint64 secs = (left + 999) / 1000;
        fail(tr("Sign in with X is cooling down. %1 seconds left.")
                 .arg(secs < 1 ? qint64(1) : secs));
        return;
    }
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
    stopLoginWait();
    closeListeners();
    const quint16 port = (quint16)CallbackPort();
    // redirect_uri is exactly http://127.0.0.1:<port>/callback. That bind must succeed.
    if (!server->listen(QHostAddress(QStringLiteral("127.0.0.1")), port)) {
        fail(tr("Cannot bind http://127.0.0.1:%1/callback — pick another -xoauthcallbackport=").arg(CallbackPort()));
        return;
    }
    // Best effort so a localhost / ::1 redirect still reaches this wallet.
    ipv6Server->listen(QHostAddress::LocalHostIPv6, port);

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
    const QString waiting = tr("Waiting for Sign in with X. finish signing in in your browser. Listening on %1 for about 5 minutes.")
                                .arg(CallbackUri());
    Q_EMIT status(waiting);
    loginWait->start((int)XOAUTH_LOGIN_WAIT_MS);
    if (!OpenSignInUrl(url)) {
        // Opening the browser failed. Do not stamp a cooldown; the user can retry now.
        fail(tr("Could not open a browser. Open this URL:\n%1").arg(url.toString()));
        return;
    }
    Q_EMIT status(waiting);
}

void XOAuth::onIncoming()
{
    QTcpServer *srv = qobject_cast<QTcpServer*>(sender());
    if (!srv)
        return;
    QTcpSocket *sock = srv->nextPendingConnection();
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
    // Idle socket only. A human login can take minutes; do not wipe PKCE or close the listener.
}

void XOAuth::onLoginWaitTimeout()
{
    if (callbackConsumed)
        return;
    MarkOAuthAttempt();
    fail(tr("Sign in with X timed out. The browser did not return to %1 within 5 minutes.")
             .arg(CallbackUri()));
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
        "<h1>X Coin</h1><p>Signed in to X Coin wallet. You can close this tab and return to the wallet.</p>"
        "<script>window.close();</script></body></html>";
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
        MarkOAuthAttempt();
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
    // Stop the 5-minute wait before the token exchange so it cannot wipe the bearer mid-flight.
    // A successful callback does not start the cooldown.
    stopLoginWait();
    closeListeners();
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
        MarkOAuthAttempt();
        fail(tr("Token request failed: %1").arg(reply->errorString()));
        return;
    }
    const QByteArray raw = reply->readAll();
    // Minimal JSON extract of access_token
    const QString s = QString::fromUtf8(raw);
    const int i = s.indexOf("\"access_token\"");
    if (i < 0) {
        MarkOAuthAttempt();
        fail(tr("Token response had no access_token. Sign-in did not succeed."));
        return;
    }
    int colon = s.indexOf(':', i);
    int q1 = s.indexOf('"', colon + 1);
    int q2 = s.indexOf('"', q1 + 1);
    if (q1 < 0 || q2 < 0) {
        MarkOAuthAttempt();
        fail(tr("Could not parse access_token"));
        return;
    }
    const QString parsed = s.mid(q1 + 1, q2 - q1 - 1);
    if (parsed.isEmpty()) {
        MarkOAuthAttempt();
        fail(tr("Empty access token — login did not succeed"));
        return;
    }
    // users/me is async. Keep this hold until that call returns.
    tokenHold.onTokenResponse(parsed.toStdString());
    fetchMe(QString::fromStdString(tokenHold.sentToUsersMe));
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
        MarkOAuthAttempt();
        fail(tr("users/me failed: %1").arg(reply->errorString()));
        return;
    }
    QString err;
    if (!finishFromUsersMe(reply->readAll(), err)) {
        MarkOAuthAttempt();
        fail(err);
    }
}

bool XOAuth::finishFromUsersMe(const QByteArray& body, QString& err)
{
    xsession::UsersMe me;
    std::string e;
    if (!xsession::ParseUsersMe(std::string(body.constData(), (size_t)body.size()), me, e)) {
        err = QString::fromStdString(e);
        return false;
    }
    // Disk proof is valid until requestShutdown deletes xsession.json.
    // exp=0 is HMAC input meaning "no wall-clock kick" (LoadSession does
    // not treat exp as a live-session TTL).
    const int64_t exp = 0;
    // Full replace of any restored xsession.json. The access token is not written.
    if (!xsession::SaveSession(me.userId, me.username, exp, e, me.verified, me.verifiedType)) {
        err = QString::fromStdString(e);
        return false;
    }
    xsession::BindLotteryFromSession();
    CacheLinkedFromUsersMe(QString::fromStdString(me.username), me.verified);
    std::string bearer;
    std::string bearerErr;
    if (!XOAuthBearerAfterUsersMe(tokenHold, bearer, bearerErr)) {
        err = QString::fromStdString(bearerErr);
        return false;
    }
    // Copy into the claim bearer before any cleanse. SaveSession does not touch it.
    xsession::SetClaimBearer(bearer);
    bearer.assign(bearer.size(), '\0');
    bearer.clear();
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
    CacheLinkedFromUsersMe(QString::fromStdString(xsession::SignedInHandle()),
                           xsession::SignedInVerified());
    Q_EMIT signedIn(QString::fromStdString(xsession::SignedInHandle()),
                    QString());
    return true;
}
