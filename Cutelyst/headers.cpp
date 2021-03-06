/*
 * Copyright (C) 2014 Daniel Nicoletti <dantti12@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "headers_p.h"

#include "common.h"

#include <QStringBuilder>
#include <QStringList>

#include <QDebug>

using namespace Cutelyst;

QString Headers::contentEncoding() const
{
    return value(QStringLiteral("content_encoding"));
}

void Headers::setContentEncoding(const QString &encoding)
{
    insert(QStringLiteral("content_encoding"), encoding);
}

QString Headers::contentType() const
{
    const QString &ct = value(QStringLiteral("content_type"));
    return ct.section(QLatin1Char(';'), 0, 0).toLower();
}

QString Headers::contentTypeCharset() const
{
    const QString &ct = value(QStringLiteral("content_type"));
    QVector<QStringRef> parts = ct.splitRef(QLatin1Char(';'));
    Q_FOREACH (const QStringRef &part, parts) {
        int pos = part.indexOf(QLatin1String("charset="));
        if (pos != -1) {
            int endPos = part.indexOf(QLatin1Char(';'), pos);
            return part.mid(pos + 8, endPos).trimmed().toString().toUpper();
        }
    }
    return QString();
}

bool Headers::contentIsText() const
{
    return value(QStringLiteral("content_type")).startsWith(QLatin1String("text/"));
}

bool Headers::contentIsHtml() const
{
    const QString &ct = contentType();
    return ct == QLatin1String("text/html") ||
            ct == QLatin1String("application/xhtml+xml") ||
            ct == QLatin1String("application/vnd.wap.xhtml+xml");
}

bool Headers::contentIsXHtml() const
{
    const QString &ct = contentType();
    return ct == QLatin1String("application/xhtml+xml") ||
            ct == QLatin1String("application/vnd.wap.xhtml+xml");
}

bool Headers::contentIsXml() const
{
    const QString &ct = contentType();
    return ct == QLatin1String("text/xml") ||
            ct == QLatin1String("application/xml") ||
            ct.endsWith(QLatin1String("xml"));
}

void Headers::setContentType(const QString &contentType)
{
    insert(QStringLiteral("content_type"), contentType);
}

qint64 Headers::contentLength() const
{
    return value(QStringLiteral("content_length")).toLongLong();
}

void Headers::setContentLength(qint64 value)
{
    insert(QStringLiteral("content_length"), QString::number(value));
}

void Headers::setDateWithDateTime(const QDateTime &date)
{
    // ALL dates must be in GMT timezone http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html
    // and follow RFC 822
    QLocale locale(QLocale::C);
    const QString &dt = locale.toString(date.toTimeSpec(Qt::UTC),
                                        QLatin1String("ddd, dd MMM yyyy hh:mm:ss")) % QLatin1String(" GMT");
    insert(QStringLiteral("date"), dt);
}

QString Headers::ifModifiedSince() const
{
    return header(QStringLiteral("if_modified_since"));
}

QDateTime Headers::ifModifiedSinceDateTime() const
{
    Headers::ConstIterator it = constFind(QStringLiteral("if_modified_since"));
    if (it == constEnd()) {
        return QDateTime();
    }

    const QString &ifModifiedStr = it.value();
    QLocale locale(QLocale::C);

    QDateTime localDT;
    if (ifModifiedStr.endsWith(QLatin1String(" GMT"))) {
        localDT = locale.toDateTime(ifModifiedStr.left(ifModifiedStr.size() - 4),
                                    QStringLiteral("ddd, dd MMM yyyy hh:mm:ss"));
    } else {
        localDT = locale.toDateTime(ifModifiedStr,
                                    QStringLiteral("ddd, dd MMM yyyy hh:mm:ss"));
    }
    return QDateTime(localDT.date(), localDT.time(), Qt::UTC);
}

QString Headers::lastModified() const
{
    return value(QStringLiteral("last_modified"));
}

void Headers::setLastModified(const QString &value)
{
    insert(QStringLiteral("last_modified"), value);
}

void Headers::setLastModified(const QDateTime &lastModified)
{
    // ALL dates must be in GMT timezone http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html
    // and follow RFC 822
    QLocale locale(QLocale::C);
    const QString &dt = locale.toString(lastModified.toTimeSpec(Qt::UTC),
                                        QStringLiteral("ddd, dd MMM yyyy hh:mm:ss")) % QLatin1String(" GMT");
    setLastModified(dt);
}

QString Headers::server() const
{
    return value(QStringLiteral("server"));
}

void Headers::setServer(const QString &value)
{
    insert(QStringLiteral("server"), value);
}

QString Headers::userAgent() const
{
    return value(QStringLiteral("user_agent"));
}

void Headers::setUserAgent(const QString &value)
{
    insert(QStringLiteral("user_agent"), value);
}

QString Headers::referer() const
{
    return value(QStringLiteral("referer"));
}

void Headers::setReferer(const QString &uri)
{
    int fragmentPos = uri.indexOf(QLatin1Char('#'));
    if (fragmentPos != -1) {
        // Strip fragment per RFC 2616, section 14.36.
        insert(QStringLiteral("referer"), uri.mid(0, fragmentPos));
    } else {
        insert(QStringLiteral("referer"), uri);
    }
}

void Headers::setWwwAuthenticate(const QString &value)
{
    insert(QStringLiteral("www_authenticate"), value);
}

void Headers::setProxyAuthenticate(const QString &value)
{
    insert(QStringLiteral("proxy_authenticate"), value);
}

QString Headers::authorization() const
{
    return value(QStringLiteral("authorization"));
}

QString Headers::authorizationBasic() const
{
    return HeadersPrivate::decodeBasicAuth(authorization());
}

QPair<QString, QString> Headers::authorizationBasicPair() const
{
    return HeadersPrivate::decodeBasicAuthPair(authorization());
}

void Headers::setAuthorizationBasic(const QString &username, const QString &password)
{
    if (username.contains(':')) {
        qCWarning(CUTELYST_CORE) << "Headers::Basic authorization user name can't contain ':'";
    }
    QString result = username % QLatin1Char(':') % password;
    insert(QStringLiteral("authorization"), QStringLiteral("Basic ") + result.toLatin1().toBase64());
}

QString Headers::proxyAuthorization() const
{
    return value(QStringLiteral("proxy_authorization"));
}

QString Headers::proxyAuthorizationBasic() const
{
    return HeadersPrivate::decodeBasicAuth(proxyAuthorization());
}

QPair<QString, QString> Headers::proxyAuthorizationBasicPair() const
{
    return HeadersPrivate::decodeBasicAuthPair(proxyAuthorization());
}

QString Headers::header(const QString &field) const
{
    return value(HeadersPrivate::normalizeHeaderKey(field));
}

QString Headers::header(const QString &field, const QString &defaultValue) const
{
    return value(HeadersPrivate::normalizeHeaderKey(field), defaultValue);
}

void Headers::setHeader(const QString &field, const QString &value)
{
    insert(HeadersPrivate::normalizeHeaderKey(field), value);
}

void Headers::setHeader(const QString &field, const QStringList &values)
{
    setHeader(field, values.join(QLatin1String(", ")));
}

void Headers::pushHeader(const QString &field, const QString &value)
{
    const QString &key = HeadersPrivate::normalizeHeaderKey(field);
    const QString &old = Headers::value(key);
    if (old.isEmpty()) {
        insert(key, value);
    } else {
        insert(key, old % QLatin1String(", ") % value);
    }
}

void Headers::pushHeader(const QString &field, const QStringList &values)
{
    pushHeader(field, values.join(QLatin1String(", ")));
}

void Headers::removeHeader(const QString &field)
{
    remove(HeadersPrivate::normalizeHeaderKey(field));
}

QString HeadersPrivate::normalizeHeaderKey(const QString &field)
{
    QString key = field;
    int i = 0;
    while (i < key.size()) {
        QCharRef c = key[i];
        if (c.isSpace()) {
            key.remove(i, 1);
            continue;
        } else if (c == QLatin1Char('-')) {
            c = QLatin1Char('_');
        } else {
            c = c.toLower();
        }
        ++i;
    }
    return key;
}

QByteArray HeadersPrivate::decodeBasicAuth(const QString &auth)
{
    if (!auth.isEmpty() && auth.startsWith(QLatin1String("Basic "))) {
        int pos = auth.lastIndexOf(' ');
        if (pos != -1) {
            return QByteArray::fromBase64(auth.mid(pos).toLatin1());
        }
    }
    return QByteArray();
}

QPair<QString, QString> HeadersPrivate::decodeBasicAuthPair(const QString &auth)
{
    const QByteArray &authorization = decodeBasicAuth(auth);
    if (!authorization.isEmpty()) {
        int pos = authorization.indexOf(':');
        if (pos == -1) {
            return qMakePair(QString::fromLatin1(authorization), QString());
        } else {
            return qMakePair(QString::fromLatin1(authorization.left(pos)),
                             QString::fromLatin1(authorization.mid(pos + 1)));
        }
    }
    return qMakePair(QString(), QString());
}
