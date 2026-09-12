// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xlookup.h"

#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "xsession.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace xlookup {

struct CacheRow {
    Status status;
    int64_t expiresAt;
};

static CCriticalSection cs_lookup;
static std::map<std::string, CacheRow> g_cache;
static std::map<std::string, Status> g_mock;

static std::string AsciiLower(const std::string& in)
{
    std::string out = in;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

static bool HandleUrlSafe(const std::string& h)
{
    if (h.empty() || h.size() > 32)
        return false;
    for (unsigned char c : h) {
        if (!(std::isalnum(c) || c == '_'))
            return false;
    }
    return true;
}

static std::string BearerToken()
{
    const std::string arg = gArgs.GetArg("-xlookupbearer", "");
    if (!arg.empty())
        return arg;
    const char* env = std::getenv("XCOIN_X_BEARER");
    if (env && env[0])
        return std::string(env);
    return std::string();
}

bool LookupEnabled()
{
    return !BearerToken().empty();
}

void SetLookupMock(const std::string& handle, Status status)
{
    std::string norm = AsciiLower(handle);
    LOCK(cs_lookup);
    g_mock[norm] = status;
    g_cache.erase(norm);
}

void ClearLookupMocks()
{
    LOCK(cs_lookup);
    g_mock.clear();
    g_cache.clear();
}

static bool DecodeChunkedBody(const std::string& in, std::string& out)
{
    out.clear();
    size_t i = 0;
    while (i < in.size()) {
        const size_t nl = in.find("\r\n", i);
        if (nl == std::string::npos)
            return false;
        char* endp = nullptr;
        const unsigned long sz = strtoul(in.c_str() + i, &endp, 16);
        if (endp == in.c_str() + i)
            return false;
        i = nl + 2;
        if (sz == 0)
            return true;
        if (i + sz > in.size())
            return false;
        out.append(in, i, sz);
        i += sz;
        if (i + 1 < in.size() && in[i] == '\r' && in[i + 1] == '\n')
            i += 2;
    }
    return !out.empty();
}

static void ExtractJsonObject(std::string& body)
{
    const size_t start = body.find('{');
    if (start == std::string::npos)
        return;
    const size_t end = body.rfind('}');
    if (end == std::string::npos || end < start)
        return;
    body = body.substr(start, end - start + 1);
}

static bool HeaderHas(const std::string& headers, const char* keyNeedle)
{
    std::string low = headers;
    for (char& c : low) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return low.find(keyNeedle) != std::string::npos;
}

static bool HttpsGetTwitter(const std::string& path, const std::string& bearer,
                            int& httpStatus, std::string& body, std::string& err)
{
    httpStatus = 0;
    body.clear();
    SSL_load_error_strings();
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        err = "ssl ctx";
        return false;
    }
    SSL_CTX_set_timeout(ctx, 8);
    SSL_CTX_set_default_verify_paths(ctx);
    BIO* bio = BIO_new_ssl_connect(ctx);
    if (!bio) {
        SSL_CTX_free(ctx);
        err = "ssl bio";
        return false;
    }
    SSL* ssl = nullptr;
    BIO_get_ssl(bio, &ssl);
    if (ssl)
        SSL_set_tlsext_host_name(ssl, "api.twitter.com");
    BIO_set_conn_hostname(bio, "api.twitter.com:443");
    if (BIO_do_connect(bio) <= 0) {
        err = "connect api.twitter.com failed";
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }
    // HTTP/1.0 avoids chunked when the peer honors it. Still dechunk below.
    std::string req = "GET " + path + " HTTP/1.0\r\n"
                      "Host: api.twitter.com\r\n"
                      "Authorization: Bearer " + bearer + "\r\n"
                      "User-Agent: xcoin-seed-xlookup/1.0\r\n"
                      "Accept: application/json\r\n"
                      "Connection: close\r\n\r\n";
    if (BIO_write(bio, req.data(), (int)req.size()) <= 0) {
        err = "write failed";
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }
    std::string raw;
    char buf[2048];
    for (;;) {
        const int n = BIO_read(bio, buf, sizeof(buf));
        if (n > 0)
            raw.append(buf, n);
        else
            break;
    }
    BIO_free_all(bio);
    SSL_CTX_free(ctx);
    if (raw.empty()) {
        err = "empty response";
        return false;
    }
    const size_t hdrEnd = raw.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) {
        err = "bad http";
        return false;
    }
    const std::string headers = raw.substr(0, hdrEnd);
    body = raw.substr(hdrEnd + 4);
    if (headers.compare(0, 5, "HTTP/") == 0) {
        const size_t sp = headers.find(' ');
        if (sp != std::string::npos)
            httpStatus = atoi(headers.c_str() + sp + 1);
    }
    if (HeaderHas(headers, "transfer-encoding: chunked")) {
        std::string decoded;
        if (DecodeChunkedBody(body, decoded))
            body.swap(decoded);
    }
    ExtractJsonObject(body);
    return true;
}

Status LookupHandle(const std::string& handle, std::string& err)
{
    err.clear();
    const std::string norm = AsciiLower(handle);
    if (!HandleUrlSafe(norm)) {
        err = "bad handle";
        return NOT_VERIFIED;
    }

    const int64_t now = GetTime();
    {
        LOCK(cs_lookup);
        auto mit = g_mock.find(norm);
        if (mit != g_mock.end())
            return mit->second;
        auto cit = g_cache.find(norm);
        if (cit != g_cache.end() && cit->second.expiresAt > now)
            return cit->second.status;
    }

    const std::string bearer = BearerToken();
    if (bearer.empty()) {
        err = "no -xlookupbearer / XCOIN_X_BEARER";
        return LOOKUP_FAILED;
    }

    int httpStatus = 0;
    std::string body;
    const std::string path = "/2/users/by/username/" + norm +
                             "?user.fields=verified,verified_type";
    if (!HttpsGetTwitter(path, bearer, httpStatus, body, err))
        return LOOKUP_FAILED;

    if (httpStatus == 404) {
        LOCK(cs_lookup);
        g_cache[norm] = {NOT_VERIFIED, now + 600};
        return NOT_VERIFIED;
    }
    if (httpStatus == 429 || httpStatus >= 500 || httpStatus == 401 || httpStatus == 403) {
        err = strprintf("x lookup http %d", httpStatus);
        return LOOKUP_FAILED;
    }
    if (httpStatus != 200) {
        LOCK(cs_lookup);
        g_cache[norm] = {NOT_VERIFIED, now + 600};
        return NOT_VERIFIED;
    }

    xsession::UsersMe me;
    std::string perr;
    if (!xsession::ParseUsersMe(body, me, perr)) {
        err = perr.empty() ? "parse users/by" : perr;
        return LOOKUP_FAILED;
    }
    const Status st = xsession::IsOfficialXVerified(me.verified, me.verifiedType)
                          ? VERIFIED
                          : NOT_VERIFIED;
    LOCK(cs_lookup);
    g_cache[norm] = {st, now + (st == VERIFIED ? 3600 : 600)};
    return st;
}

} // namespace xlookup
