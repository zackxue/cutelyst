/*
 * Copyright (C) 2013-2015 Daniel Nicoletti <dantti12@gmail.com>
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

#include "session_p.h"
#include "application.h"
#include "request.h"
#include "response.h"
#include "context.h"
#include "engine.h"

#include <QtCore/QStringBuilder>
#include <QtCore/QSettings>
#include <QtCore/QUuid>
#include <QtCore/QDir>
#include <QtCore/QLoggingCategory>
#include <QtCore/QCoreApplication>
#include <QtNetwork/QNetworkCookie>

using namespace Cutelyst;

Q_LOGGING_CATEGORY(C_SESSION, "cutelyst.plugin.session")

#define SESSION_VALUES "__session_values"
#define SESSION_SAVE "__session_save"
#define SESSION_ID "__session_id"

Session::Session(Application *parent) : Plugin(parent)
  , d_ptr(new SessionPrivate)
{
    d_ptr->q_ptr = this;
}

Cutelyst::Session::~Session()
{
    delete d_ptr;
}

bool Session::setup(Application *app)
{
    Q_D(Session);
    d->sessionName = QCoreApplication::applicationName() % QStringLiteral("_session");
    connect(app, &Application::afterDispatch, d, &SessionPrivate::saveSession);
    return true;
}

QVariant Session::value(Cutelyst::Context *c, const QString &key, const QVariant &defaultValue)
{
    const QVariant &data = SessionPrivate::loadSession(c);
    if (data.isNull()) {
        return defaultValue;
    }

    const QVariantHash &session = data.toHash();
    return session.value(key, defaultValue);
}

void Session::setValue(Cutelyst::Context *c, const QString &key, const QVariant &value)
{
    QVariantHash session = SessionPrivate::loadSession(c).toHash();
    if (value.isNull()) {
        session.remove(key);
    } else {
        session.insert(key, value);
    }
    c->setProperty(SESSION_VALUES, session);
    c->setProperty(SESSION_SAVE, true);
}

void Session::deleteValue(Context *c, const QString &key)
{
    setValue(c, key, QVariant());
}

bool Session::isValid(Cutelyst::Context *c)
{
    return !SessionPrivate::loadSession(c).isNull();
}

QVariantHash Session::retrieveSession(const QString &sessionId) const
{
    QVariantHash ret;
    const QString &sessionFile = SessionPrivate::filePath(sessionId);
    if (QFileInfo::exists(sessionFile)) {
        qCDebug(C_SESSION) << "Retrieving session values from" << sessionFile;

        QSettings settings(sessionFile, QSettings::IniFormat);
        settings.beginGroup(QLatin1String("Data"));
        Q_FOREACH (const QString &key, settings.allKeys()) {
            ret.insert(key, settings.value(key));
        }
        settings.endGroup();
    }
    return ret;
}

void Session::persistSession(const QString &sessionId, const QVariant &data) const
{
    const QString &sessionFile = SessionPrivate::filePath(sessionId);
    const QVariantHash &hash = data.toHash();
    if (hash.isEmpty()) {
        qCDebug(C_SESSION) << "Clearing session values on" << sessionFile;
        bool ret = QFile::remove(sessionFile);
        if (!ret) {
            QSettings settings(sessionFile, QSettings::IniFormat);
            settings.clear();
        }
    } else {
        qCDebug(C_SESSION) << "Persisting session values to" << sessionFile;
        QSettings settings(sessionFile, QSettings::IniFormat);
        settings.beginGroup(QLatin1String("Data"));
        QVariantHash::ConstIterator it = hash.constBegin();
        while (it != hash.constEnd()) {
            if (it.value().isNull()) {
                settings.remove(it.key());
            } else {
                settings.setValue(it.key(), it.value());
            }
            ++it;

        }
        settings.endGroup();
    }
}

QString SessionPrivate::filePath(const QString &sessionId)
{
    QString path = QDir::tempPath() % QLatin1Char('/') % QCoreApplication::applicationName();
    QDir dir;
    if (!dir.mkpath(path)) {
        qCWarning(C_SESSION) << "Failed to create path for session object" << path;
    }
    return path % QLatin1Char('/') % sessionId;
}

QString SessionPrivate::generateSessionId()
{
    return QUuid::createUuid().toString();
}

QString SessionPrivate::getSessionId(Context *c, const QString &sessionName, bool create)
{
    const QVariant &property = c->property(SESSION_ID);
    if (!property.isNull()) {
        return property.toString();
    }

    QString sessionId;
    Q_FOREACH (const QNetworkCookie &cookie, c->req()->cookies()) {
        if (cookie.name() == sessionName) {
            sessionId = cookie.value();
            qCDebug(C_SESSION) << "Found sessionid" << sessionId << "in cookie";
            break;
        }
    }

    if (sessionId.isEmpty()) {
        if (create) {
            sessionId = generateSessionId();
            qCDebug(C_SESSION) << "Created session" << sessionId;
            c->setProperty(SESSION_ID, sessionId);
        }
    } else {
        c->setProperty(SESSION_ID, sessionId);
    }

    return sessionId;
}

void SessionPrivate::saveSession(Context *c)
{
    if (!c->property(SESSION_SAVE).toBool()) {
        return;
    }

    Session *session = c->plugin<Session*>();
    if (!session) {
        qCCritical(C_SESSION) << "Session plugin not registered";
        return;
    }

    const QString &_sessionName = session->d_ptr->sessionName;
    const QString &sessionId = getSessionId(c, _sessionName, true);
    QNetworkCookie sessionCookie(_sessionName.toLatin1(),
                                 sessionId.toLatin1());
    c->res()->addCookie(sessionCookie);
    session->persistSession(sessionId, loadSession(c));
}

QVariant SessionPrivate::loadSession(Context *c)
{
    const QVariant &property = c->property(SESSION_VALUES);
    if (!property.isNull()) {
        return property.toHash();
    }

    Session *session = c->plugin<Session*>();
    if (!session) {
        qCCritical(C_SESSION) << "Session plugin not registered";
        return QVariant();
    }

    const QString &sessionid = getSessionId(c, session->d_ptr->sessionName, false);
    if (!sessionid.isEmpty()) {
        const QVariantHash &sessionHash = session->retrieveSession(sessionid);
        c->setProperty(SESSION_VALUES, sessionHash);
        return sessionHash;
    }

    c->setProperty(SESSION_VALUES, QVariantHash());
    return QVariantHash();
}
