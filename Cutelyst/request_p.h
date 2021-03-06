/*
 * Copyright (C) 2013 Daniel Nicoletti <dantti12@gmail.com>
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

#ifndef CUTELYST_REQUEST_P_H
#define CUTELYST_REQUEST_P_H

#include "request.h"

#include "upload.h"

#include <QtCore/QStringList>
#include <QtCore/QUrlQuery>
#include <QtCore/QUrl>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkCookie>

namespace Cutelyst {

class Engine;
class RequestPrivate
{
public:
    // call reset before reusing it
    void reset();

    void parseUrlQuery() const;
    void parseBody() const;
    void parseCookies() const;

    // Manually filled by the Engine
    QString method;
    QString protocol;
    Headers headers;
    QIODevice *body = 0;
    QHostAddress remoteAddress;
    mutable QString remoteHostname;
    quint16 remotePort;
    QString remoteUser;
    Engine *engine;
    quint64 startOfRequest;
    quint64 endOfRequest;
    // Pointer to Engine data
    void *requestPtr = 0;

    bool https = false;
    // Path must not have a leading slash
    QString path;
    QByteArray query;
    QString serverAddress;

protected:
    friend class Request;
    friend class Dispatcher;
    friend class DispatchType;

    static ParamsMultiMap parseUrlEncoded(const QByteArray &line);

    // Engines don't need to touch this
    QStringList args;
    QStringList captures;
    QString match;

    mutable bool urlParsed = false;
    mutable QUrl url;

    mutable bool baseParsed = false;
    mutable QString base;

    mutable bool cookiesParsed = false;
    mutable QList<QNetworkCookie> cookies;

    mutable bool queryParamParsed = false;
    mutable ParamsMultiMap queryParam;
    mutable QString queryKeywords;

    mutable bool bodyParsed = false;
    mutable ParamsMultiMap bodyParam;
    mutable QVariant bodyData;

    mutable bool paramParsed = false;
    mutable ParamsMultiMap param;

    mutable QMap<QString, Upload *> uploads;
};

}

#endif // CUTELYST_REQUEST_P_H
