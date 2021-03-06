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

#include "dispatchtype.h"

#include "context_p.h"

using namespace Cutelyst;

DispatchType::DispatchType(QObject *parent) :
    QObject(parent)
{
}

DispatchType::~DispatchType()
{
}

QString DispatchType::uriForAction(Action *action, const QStringList &captures) const
{
    Q_UNUSED(action)
    Q_UNUSED(captures)
    return QString();
}

bool DispatchType::registerAction(Action *action)
{
    Q_UNUSED(action)
    return true;
}

bool DispatchType::isLowPrecedence() const
{
    return false;
}

void DispatchType::setupMatchedAction(Context *c, Action *action) const
{
    c->d_ptr->action = action;
}
