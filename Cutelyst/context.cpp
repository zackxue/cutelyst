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

#include "context_p.h"

#include "common.h"
#include "request.h"
#include "response.h"
#include "action.h"
#include "dispatcher.h"
#include "controller.h"
#include "application.h"
#include "stats.h"

#include "config.h"

#include <QUrl>
#include <QUrlQuery>
#include <QStringBuilder>
#include <QCoreApplication>

using namespace Cutelyst;

Context::Context(ContextPrivate *priv) :
    d_ptr(priv)
{
}

Context::~Context()
{
    delete d_ptr;
}

bool Context::error() const
{
    Q_D(const Context);
    return !d->error.isEmpty();
}

void Context::error(const QString &error)
{
    Q_D(Context);
    if (!error.isEmpty()) {
        d->error << error;
    }
}

QStringList Context::errors() const
{
    Q_D(const Context);
    return d->error;
}

bool Context::state() const
{
    Q_D(const Context);
    return d->state;
}

void Context::setState(bool state)
{
    Q_D(Context);
    d->state = state;
}

Engine *Context::engine() const
{
    Q_D(const Context);
    return d->engine;
}

Application *Context::app() const
{
    Q_D(const Context);
    return d->app;
}

Response *Context::response() const
{
    Q_D(const Context);
    return d->response;
}

Response *Context::res() const
{
    Q_D(const Context);
    return d->response;
}

Action *Context::action() const
{
    Q_D(const Context);
    return d->action;
}

QString Context::actionName() const
{
    Q_D(const Context);
    return d->action->name();
}

QString Context::ns() const
{
    Q_D(const Context);
    return d->action->ns();
}

Request *Context::request() const
{
    Q_D(const Context);
    return d->request;
}

Request *Context::req() const
{
    Q_D(const Context);
    return d->request;
}

Dispatcher *Context::dispatcher() const
{
    Q_D(const Context);
    return d->dispatcher;
}

QString Cutelyst::Context::controllerName() const
{
    Q_D(const Context);
    return d->action->controller()->metaObject()->className();
}

Controller *Context::controller() const
{
    Q_D(const Context);
    return d->action->controller();
}

Controller *Context::controller(const QString &name) const
{
    Q_D(const Context);
    return d->dispatcher->controllers().value(name);
}

View *Context::view() const
{
    Q_D(const Context);
    return d->view;
}

bool Context::setView(const QString &name)
{
    Q_D(Context);
    return d->view = d->app->view(name);
}

QVariantHash &Context::stash()
{
    Q_D(Context);
    return d->stash;
}

QVariant Context::stash(const QString &key) const
{
    Q_D(const Context);
    return d->stash.value(key);
}

void Context::setStash(const QString &key, const QVariant &value)
{
    Q_D(Context);
    d->stash.insert(key, value);
}

QStack<Component *> Context::stack() const
{
    Q_D(const Context);
    return d->stack;
}

QUrl Context::uriFor(const QString &path, const QStringList &args, const ParamsMultiMap &queryValues) const
{
    Q_D(const Context);

    QUrl ret = d->request->uri();

    QString _path = path;
    if (_path.isEmpty()) {
        _path = d->action->controller()->ns();
    }

    if (!args.isEmpty()) {
        QStringList encodedArgs;
        encodedArgs.append(_path);

        Q_FOREACH (const QString &arg, args) {
            encodedArgs.append(QUrl::toPercentEncoding(arg));
        }
        ret.setPath(QLatin1Char('/') % encodedArgs.join(QLatin1Char('/')));
    } else if (_path.startsWith(QLatin1Char('/'))) {
        ret.setPath(_path);
    } else {
        ret.setPath(QLatin1Char('/') % _path);
    }

    if (!queryValues.isEmpty()) {
        // Avoid a trailing '?'
        QUrlQuery query;
        if (queryValues.size()) {
            ParamsMultiMap::ConstIterator i = queryValues.constBegin();
            while (i != queryValues.constEnd()) {
                query.addQueryItem(i.key(), i.value());
                ++i;
            }
        }
        ret.setQuery(query);
    }

    return ret;
}

QUrl Context::uriFor(Action *action, const QStringList &args, const ParamsMultiMap &queryValues) const
{
    return uriForWithCaptures(action, QStringList(), args, queryValues);
}

QUrl Context::uriForWithCaptures(Action *action, const QStringList &captures, const QStringList &args, const ParamsMultiMap &queryValues) const
{
    Q_D(const Context);

    QString path;
    if (action) {
        path = d->dispatcher->uriForAction(action, captures);
    } else {
        path = d->dispatcher->uriForAction(d->action, captures);
    }

    return uriFor(path, args, queryValues);
}

QUrl Context::uriForAction(const QString &path, const QStringList &args, const ParamsMultiMap &queryValues) const
{
    Q_D(const Context);

    Action *action = d->dispatcher->getActionByPath(path);
    return uriFor(action, args, queryValues);
}

bool Context::detached() const
{
    Q_D(const Context);
    return d->detached;
}

void Context::detach(Action *action)
{
    Q_D(Context);
    if (action) {
        d->dispatcher->forward(this, action);
    }
    d->detached = true;
}

bool Context::forward(Component *action)
{
    Q_D(Context);
    return d->dispatcher->forward(this, action);
}

bool Context::forward(const QString &action)
{
    Q_D(Context);
    return d->dispatcher->forward(this, action);
}

Action *Context::getAction(const QString &action, const QString &ns)
{
    Q_D(Context);
    return d->dispatcher->getAction(action, ns);
}

QList<Action *> Context::getActions(const QString &action, const QString &ns)
{
    Q_D(Context);
    return d->dispatcher->getActions(action, ns);
}

QList<Cutelyst::Plugin *> Context::plugins()
{
    Q_D(Context);
    return d->plugins;
}

bool Context::execute(Component *code)
{
    Q_D(Context);
    Q_ASSERT_X(code, "Context::execute", "trying to execute a null Cutelyst::Component");

    bool ret;
    d->stack.push(code);

    if (d->stats) {
        QString statsInfo = d->statsStartExecute(code);

        ret = code->execute(this);

        if (!statsInfo.isNull()) {
            d->statsFinishExecute(statsInfo);
        }
    } else {
        ret = code->execute(this);
    }

    d->stack.pop();

    return ret;
}

QVariant Context::config(const QString &key, const QVariant &defaultValue) const
{
    Q_D(const Context);
    return d->app->config(key, defaultValue);
}

QVariantHash Context::config() const
{
    Q_D(const Context);
    return d->app->config();
}

QByteArray Context::welcomeMessage() const
{
    QString name = QCoreApplication::applicationName();
    response()->setContentType(QStringLiteral("text/html; charset=utf-8"));
    QByteArray ret;
    QTextStream out(&ret);
    out << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n";
    out << "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n";
    out << "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n";
    out << "    <head>\n";
    out << "        <meta http-equiv=\"Content-Language\" content=\"en\" />\n";
    out << "        <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n";
    out << "        <title>" << name << " on Cutelyst " << QStringLiteral(VERSION) << "</title>\n";
    out << "    </head>\n";
    out << "    <body>\n";
    out << "        <div id=\"content\">\n";
    out << "            <div id=\"topbar\">\n";
    out << "               <h1><span id=\"appname\">" << name << "</span> on <a href=\"http://cutelyst.org\">Cutelyst</a>\n";
    out << "                   " << QStringLiteral(VERSION) << "</h1>\n";
    out << "             </div>\n";
    out << "         </div>\n";
    out << "    <body>\n";
    out << "</html>\n";
    return ret;
}

void *Context::engineData()
{
    Q_D(const Context);
    return d->requestPtr;
}

QString ContextPrivate::statsStartExecute(Component *code)
{
    // Skip internal actions
    if (code->name().startsWith(QLatin1Char('_'))) {
        return QString();
    }

    QString actionName = code->reverse();

    if (dynamic_cast<Action *>(code)) {
        actionName = QLatin1Char('/') % actionName;
    }

    if (stack.size() > 2) {
        actionName = QLatin1String("-> ") % actionName;
        actionName = actionName.rightJustified(actionName.size() + stack.size() - 2, QLatin1Char(' '));
    }

    stats->profileStart(actionName);

    return actionName;
}


void ContextPrivate::statsFinishExecute(const QString &statsInfo)
{
    stats->profileEnd(statsInfo);
}
