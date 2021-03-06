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

#ifndef CUTELYST_DISPATCHER_P_H
#define CUTELYST_DISPATCHER_P_H

#include "dispatcher.h"

namespace Cutelyst {

class DispatcherPrivate
{
    Q_DECLARE_PUBLIC(Dispatcher)
public:
    DispatcherPrivate(Dispatcher *q);

    void printActions() const;
    ActionList getContainers(const QString &ns) const;
    Action *command2Action(Context *c, const QString &command, const QStringList &args) const;
    Action *invokeAsPath(Context *c, const QString &relativePath, const QStringList &args) const;
    static QString actionRel2Abs(Context *c, const QString &path);
    static QString cleanNamespace(const QString &ns);

    Dispatcher *q_ptr;
    bool showInternalActions = false;
    QHash<QString, Action*> actionHash;
    QHash<QString, ActionList> containerHash;
    ActionList rootActions;
    QHash<QString, Controller *> constrollerHash;
    QList<DispatchType*> dispatchers;
};

}

#endif // CUTELYST_DISPATCHER_P_H
