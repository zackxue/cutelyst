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

#include "roleacl_p.h"

#include "common.h"

#include <Cutelyst/Plugins/authentication.h>
#include <Cutelyst/Controller>
#include <Cutelyst/Dispatcher>

#include <QMap>

using namespace Cutelyst;

RoleACL::RoleACL() :
    d_ptr(new RoleACLPrivate)
{
}

RoleACL::~RoleACL()
{
    delete d_ptr;
}

Component::Modifiers RoleACL::modifiers() const
{
    return AroundExecute;
}

bool RoleACL::init(Cutelyst::Application *application, const QVariantHash &args)
{
    Q_D(RoleACL);
    Q_UNUSED(application)

    QMap<QString, QString> attributes;
    attributes = args.value("attributes").value<QMap<QString, QString> >();
    d->actionReverse = args.value("reverse").toByteArray();

    if (!attributes.contains("RequiresRole") && !attributes.contains("AllowedRole")) {
        qCritical() << "Action"
                    << d->actionReverse
                    << "requires at least one RequiresRole or AllowedRole attribute";
        return false;
    } else {
        QStringList required = attributes.values("RequiresRole");
        Q_FOREACH (const QString &role, required) {
            d->requiresRole.append(role);
        }

        QStringList allowed = attributes.values("AllowedRole");
        Q_FOREACH (const QString &role, allowed) {
            d->allowedRole.append(role);
        }
    }

    if (!attributes.contains("ACLDetachTo") && !attributes.value("ACLDetachTo").isEmpty()) {
        qCritical() << "Action"
                    << d->actionReverse
                    << "requires the ACLDetachTo(<action>) attribute";
        return false;
    }
    d->aclDetachTo = attributes.value("ACLDetachTo");

    return true;
}

bool RoleACL::aroundExecute(Context *c, QStack<Cutelyst::Component *> stack)
{
    Q_D(const RoleACL);

    if (canVisit(c)) {
        return Component::aroundExecute(c, stack);
    }

    c->detach(d->detachTo);

    return false;
}

bool RoleACL::canVisit(Context *c) const
{
    Q_D(const RoleACL);

    Authentication *auth = c->plugin<Authentication*>();
    if (auth) {
        QStringList user_has = auth->user(c).values(QStringLiteral("roles"));

        QStringList required = d->requiresRole;
        QStringList allowed = d->allowedRole;

        if (!required.isEmpty() && !allowed.isEmpty()) {
            Q_FOREACH (const QString &role, required) {
                if (!user_has.contains(role)) {
                    return false;
                }
            }
            Q_FOREACH (const QString &role, allowed) {
                if (user_has.contains(role)) {
                    return true;
                }
            }
            return false;
        }  else if (!required.isEmpty()) {
            Q_FOREACH (const QString &role, required) {
                if (!user_has.contains(role)) {
                    return false;
                }
            }
            return true;
        } else if (!allowed.isEmpty()) {
            Q_FOREACH (const QString &role, allowed) {
                if (user_has.contains(role)) {
                    return true;
                }
            }
            return false;
        }
    }

    return false;
}

bool RoleACL::dispatcherReady(const Dispatcher *dispatcher, Cutelyst::Controller *controller)
{
    Q_D(RoleACL);
    Q_UNUSED(controller)

    d->detachTo = dispatcher->getAction(d->aclDetachTo);
    if (!d->detachTo) {
        qCritical() << "Action"
                    << d->actionReverse
                    << "requires a valid action set on the ACLDetachTo("
                    << d->aclDetachTo.data()
                    << ") attribute";
        return false;
    }

    return true;
}
