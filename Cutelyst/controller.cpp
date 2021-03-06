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

#include "controller_p.h"

#include "application.h"
#include "dispatcher.h"
#include "action.h"
#include "common.h"

#include <QMetaClassInfo>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QDebug>

using namespace Cutelyst;

Controller::Controller(QObject *parent) :
    QObject(parent),
    d_ptr(new ControllerPrivate(this))
{
}

Controller::~Controller()
{
    Q_D(Controller);
    qDeleteAll(d->actions);
    delete d_ptr;
}

QString Controller::ns() const
{
    Q_D(const Controller);
    return d->pathPrefix;
}

Action *Controller::actionFor(const QString &name) const
{
    Q_D(const Controller);
    Action *ret = d->actions.value(name);
    if (ret) {
        return ret;
    }
    return d->dispatcher->getAction(name, d->pathPrefix);
}

ActionList Controller::actions() const
{
    Q_D(const Controller);
    return d->actions.values();
}

bool Controller::operator==(const char *className)
{
    return !qstrcmp(metaObject()->className(), className);
}

void Controller::Begin(Context *c)
{
    Q_UNUSED(c)
}

bool Controller::Auto(Context *c)
{
    Q_UNUSED(c)
    return true;
}

void Controller::End(Context *c)
{
    Q_UNUSED(c)
}

bool Controller::preFork(Application *app)
{
    Q_UNUSED(app)
    return true;
}

bool Controller::postFork(Application *app)
{
    Q_UNUSED(app)
    return true;
}

bool Controller::_BEGIN(Context *c)
{
//    qDebug() << Q_FUNC_INFO;
    Q_D(Controller);
    if (d->begin) {
        d->begin->dispatch(c);
        return !c->error();
    }
    return true;
}

bool Controller::_AUTO(Context *c)
{
//    qDebug() << Q_FUNC_INFO;
    Q_D(Controller);
    Q_FOREACH (Action *autoAction, d->autoList) {
        if (!autoAction->dispatch(c)) {
            return false;
        }
    }
    return true;
}

bool Controller::_ACTION(Context *c)
{
//    qDebug() << Q_FUNC_INFO;
    if (c->action()) {
        return c->action()->dispatch(c);
    }
    return !c->error();
}

bool Controller::_END(Context *c)
{
//    qDebug() << Q_FUNC_INFO;
    Q_D(Controller);
    if (d->end) {
        d->end->dispatch(c);
        return !c->error();
    }
    return true;
}


ControllerPrivate::ControllerPrivate(Controller *parent) :
    q_ptr(parent)
{
}

void ControllerPrivate::init(Application *app, Dispatcher *_dispatcher)
{
    Q_Q(Controller);

    dispatcher = _dispatcher;

    // Application must always be our parent
    q->setParent(app);

    const QMetaObject *meta = q->metaObject();
    const QString &className = QString::fromLatin1(meta->className());
    q->setObjectName(className);

    QByteArray controlerNS;
    for (int i = 0; i < meta->classInfoCount(); ++i) {
        if (meta->classInfo(i).name() == QLatin1String("Namespace")) {
            controlerNS = meta->classInfo(i).value();
            break;
        }
    }

    if (controlerNS.isNull()) {
        bool lastWasUpper = true;

        for (int i = 0; i < className.length(); ++i) {
            if (className.at(i).toLower() == className.at(i)) {
                controlerNS.append(className.at(i));
                lastWasUpper = false;
            } else {
                if (lastWasUpper) {
                    controlerNS.append(className.at(i).toLower());
                } else {
                    controlerNS.append(QLatin1Char('/') % className.at(i).toLower());
                }
                lastWasUpper = true;
            }
        }
    }
    pathPrefix = controlerNS;

    registerActionMethods(meta, q, app);
}

void ControllerPrivate::setupFinished()
{
    Q_Q(Controller);

    const ActionList &beginList = dispatcher->getActions(QStringLiteral("Begin"), pathPrefix);
    if (!beginList.isEmpty()) {
        begin = beginList.last();
        actionSteps.append(begin);
    }

    autoList = dispatcher->getActions(QStringLiteral("Auto"), pathPrefix);
    actionSteps.append(autoList);

    const ActionList &endList = dispatcher->getActions(QStringLiteral("End"), pathPrefix);
    if (!endList.isEmpty()) {
        end = endList.last();
    }

    Q_FOREACH (Action *action, actions.values()) {
        action->dispatcherReady(dispatcher, q);
    }

    q->preFork(qobject_cast<Application *>(q->parent()));
}

bool Controller::_DISPATCH(Context *c)
{
    Q_D(Controller);

    bool failedState = false;

    // Dispatch to _BEGIN and _AUTO
    Q_FOREACH (Action *action, d->actionSteps) {
        if (!action->dispatch(c)) {
            failedState = true;
            break;
        }
    }

    // Dispatch to _ACTION
    if (!failedState) {
        c->action()->dispatch(c);
    }

    // Dispatch to _END
    if (d->end) {
        d->end->dispatch(c);
    }

    return c->state();
}

Action *ControllerPrivate::actionClass(const QVariantHash &args)
{
    QMap<QString, QString> attributes;
    attributes = args.value("attributes").value<QMap<QString, QString> >();
    QString actionClass = attributes.value("ActionClass");

    QObject *object = instantiateClass(actionClass.toLatin1(), "Cutelyst::Action");
    if (object) {
        Action *action = qobject_cast<Action*>(object);
        if (action) {
            return qobject_cast<Action*>(object);
        }
        qCWarning(CUTELYST_CONTROLLER) << "ActionClass"
                                       << actionClass
                                       << "is not an ActionClass";
        delete object;
    }

    return new Action;
}

Action *ControllerPrivate::createAction(const QVariantHash &args, const QMetaMethod &method, Controller *controller, Application *app)
{
    Action *action = actionClass(args);
    if (!action) {
        return 0;
    }

    QString name = args.value("name").toString();
    QRegularExpression regex(QStringLiteral("^_(DISPATCH|BEGIN|AUTO|ACTION|END)$"));
    QRegularExpressionMatch match = regex.match(name);
    if (!match.hasMatch()) {
        QStack<Component *> roles = gatherActionRoles(args);
        for (int i = 0; i < roles.size(); ++i) {
            Component *code = roles.at(i);
            code->init(app, args);
            code->setParent(action);
        }
        action->applyRoles(roles);
    }

    action->setMethod(method);
    action->setController(controller);
    action->setName(args.value("name").toString());
    action->setReverse(args.value("reverse").toString());
    action->setupAction(args, app);

    return action;
}

void ControllerPrivate::registerActionMethods(const QMetaObject *meta, Controller *controller, Application *app)
{
    // Setup actions
    for (int i = 0; i < meta->methodCount(); ++i) {
        const QMetaMethod &method = meta->method(i);
        const QByteArray &name = method.name();

        // We register actions that are either a Q_SLOT
        // or a Q_INVOKABLE function which has the first
        // parameter type equal to Context*
        if (method.isValid() &&
                (method.methodType() == QMetaMethod::Method || method.methodType() == QMetaMethod::Slot) &&
                (method.parameterCount() && method.parameterType(0) == qMetaTypeId<Cutelyst::Context *>())) {

            // Build up the list of attributes for the class info
            QByteArray attributeArray;
            for (int i = 0; i < meta->classInfoCount(); ++i) {
                QMetaClassInfo classInfo = meta->classInfo(i);
                if (name == classInfo.name()) {
                    attributeArray.append(classInfo.value());
                }
            }
            QMap<QString, QString> attrs = parseAttributes(method, attributeArray, name);

            QString reverse;
            if (controller->ns().isEmpty()) {
                reverse = name;
            } else {
                reverse = controller->ns() % QLatin1Char('/') % name;
            }

            Action *action = createAction({
                                              {"name"      , QVariant::fromValue(name)},
                                              {"reverse"   , QVariant::fromValue(reverse)},
                                              {"namespace" , QVariant::fromValue(controller->ns())},
                                              {"attributes", QVariant::fromValue(attrs)}
                                          },
                                          method,
                                          controller,
                                          app);

            actions.insertMulti(action->reverse(), action);
        }
    }
}

QMap<QString, QString> ControllerPrivate::parseAttributes(const QMetaMethod &method, const QByteArray &str, const QByteArray &name)
{
    QList<QPair<QString, QString> > attributes;
    // This is probably not the best parser ever
    // but it handles cases like:
    // :Args:Local('fo"')o'):ActionClass('foo')
    // into
    // (QPair("Args",""), QPair("Local","'fo"')o'"), QPair("ActionClass","'foo'"))

    int size = str.size();
    int pos = 0;
    while (pos < size) {
        QString key;
        QString value;

        // find the start of a key
        if (str.at(pos) == ':') {
            int keyStart = ++pos;
            int keyLength = 0;
            while (pos < size) {
                if (str.at(pos) == '(') {
                    // attribute has value
                    int valueStart = ++pos;
                    while (pos < size) {
                        if (str.at(pos) == ')') {
                            // found the possible end of the value
                            int valueEnd = pos;
                            if (++pos < size && str.at(pos) == ':') {
                                // found the start of a key so this is
                                // really the end of a value
                                value = str.mid(valueStart, valueEnd - valueStart);
                                break;
                            } else if (pos >= size) {
                                // found the end of the string
                                // save the remainig as the value
                                value = str.mid(valueStart, valueEnd - valueStart);
                                break;
                            }
                            // string was not '):' or ')$'
                            continue;
                        }
                        ++pos;
                    }

                    break;
                } else if (str.at(pos) == ':') {
                    // Attribute has no value
                    break;
                }
                ++keyLength;
                ++pos;
            }

            // stopre the key
            key = str.mid(keyStart, keyLength);

            // remove quotes
            if (!value.isEmpty()) {
                if ((value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\''))) ||
                        (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))) {
                    value.remove(0, 1);
                    value.remove(value.size() - 1, 1);
                }
            }

            // store the key/value pair found
            attributes.append(qMakePair(key, value));
            continue;
        }
        ++pos;
    }

    QMap<QString, QString> ret;
    // Add the attributes to the hash in the reverse order so
    // that values() return them in the right order
    for (int i = attributes.size() - 1; i >= 0; --i) {
        const QPair<QString, QString> &pair = attributes.at(i);
        QString key = pair.first;
        QString value = pair.second;
        if (key == QLatin1String("Global")) {
            key = QStringLiteral("Path");
            value = parsePathAttr(QLatin1Char('/') % name);
        } else if (key == QLatin1String("Local")) {
            key = QStringLiteral("Path");
            value = parsePathAttr(name);
        } else if (key == QLatin1String("Path")) {
            value = parsePathAttr(value);
        } else if (key == QLatin1String("Args")) {
            QString args = value;
            if (!args.isEmpty()) {
                value = args.remove(QRegularExpression("\\D")).toLocal8Bit();
            }
        } else if (key == QLatin1String("CaptureArgs")) {
            QString captureArgs = value;
            value = captureArgs.remove(QRegularExpression("\\D")).toLocal8Bit();
        } else if (key == QLatin1String("Chained")) {
            value = parseChainedAttr(value);
        }

        ret.insertMulti(key, value);
    }

    // Handle special AutoArgs and AutoCaptureArgs case
    if (!ret.contains(QLatin1String("Args")) && !ret.contains(QLatin1String("CaptureArgs")) &&
            (ret.contains(QLatin1String("AutoArgs")) || ret.contains(QLatin1String("AutoCaptureArgs")))) {
        if (ret.contains(QLatin1String("AutoArgs")) && ret.contains(QLatin1String("AutoCaptureArgs"))) {
            qFatal("Action '%s' has both AutoArgs and AutoCaptureArgs, which is not allowed", name.data());
        } else {
            QString parameterName;
            if (ret.contains(QLatin1String("AutoArgs"))) {
                ret.remove(QLatin1String("AutoArgs"));
                parameterName  = QStringLiteral("Args");
            } else {
                ret.remove(QLatin1String("AutoCaptureArgs"));
                parameterName  = QStringLiteral("CaptureArgs");
            }

            // If the signature is not QStringList we count them
            if (!(method.parameterCount() == 2 && method.parameterType(1) == QMetaType::QStringList)) {
                int parameterCount = 0;
                for (int i = 1; i < method.parameterCount(); ++i) {
                    int typeId = method.parameterType(i);
                    if (typeId == QMetaType::QString) {
                        ++parameterCount;
                    }
                }
                ret.insert(parameterName, QString::number(parameterCount));
            }
        }

    }

    // If the method is private add a Private attribute
    if (!ret.contains(QStringLiteral("Private")) && method.access() == QMetaMethod::Private) {
        ret.insert(QStringLiteral("Private"), QByteArray());
    }

    return ret;
}

QStack<Component *> ControllerPrivate::gatherActionRoles(const QVariantHash &args)
{
    QStack<Component *> roles;
    QMap<QByteArray, QByteArray> attributes;
    attributes = args.value("attributes").value<QMap<QByteArray, QByteArray> >();
    Q_FOREACH (const QByteArray &role, attributes.values("Does")) {
        QObject *object = instantiateClass(role, "Cutelyst::Component");
        if (object) {
            roles.push(qobject_cast<Component *>(object));
        }
    }
    return roles;
}

QString ControllerPrivate::parsePathAttr(const QString &_value)
{
    QString value = _value;
    if (value.isNull()) {
        value = QStringLiteral("");
    }

    if (value.startsWith(QLatin1Char('/'))) {
        return value;
    } else if (value.length()) {
        return pathPrefix % QLatin1Char('/') % value;
    }
    return pathPrefix;
}

QString ControllerPrivate::parseChainedAttr(const QString &attr)
{
    if (attr.isEmpty()) {
        return QStringLiteral("/");
    }

    if (attr == QLatin1String(".")) {
        return QLatin1Char('/') % pathPrefix;
    } else if (!attr.startsWith(QLatin1Char('/'))) {
        if (!pathPrefix.isEmpty()) {
            return QLatin1Char('/') % pathPrefix % QLatin1Char('/') % attr;
        } else {
            // special case namespace '' (root)
            return QLatin1Char('/') % attr;
        }
    }

    return attr;
}

QObject *ControllerPrivate::instantiateClass(const QByteArray &name, const QByteArray &super)
{
    QString instanceName = name;
    if (!instanceName.isEmpty()) {
        instanceName.remove(QRegularExpression("\\W"));

        int id = QMetaType::type(instanceName.toLocal8Bit().data());
        if (!id) {
            if (!instanceName.endsWith(QLatin1Char('*'))) {
                instanceName.append("*");
            }

            id = QMetaType::type(instanceName.toLocal8Bit().data());
            if (!id && !instanceName.startsWith(QLatin1String("Cutelyst::"))) {
                instanceName = QLatin1String("Cutelyst::") % instanceName;
                id = QMetaType::type(instanceName.toLocal8Bit().data());
            }
        }

        if (id) {
            const QMetaObject *metaObj = QMetaType::metaObjectForType(id);
            if (metaObj) {
                if (!superIsClassName(metaObj->superClass(), super)) {
                    qCWarning(CUTELYST_CONTROLLER)
                            << "Class name"
                            << instanceName
                            << "is not a derived class of"
                            << super;
                }

                QObject *object = metaObj->newInstance();
                if (!object) {
                    qCWarning(CUTELYST_CONTROLLER)
                            << "Could create a new instance of"
                            << instanceName
                            << "make sure it's default constructor is "
                               "marked with the Q_INVOKABLE macro";
                }

                return object;
            }
        }

        if (!id) {
            qCCritical(CUTELYST_CONTROLLER)
                    << "Class name"
                    << instanceName
                    << "is not registerd, you can register it with qRegisterMetaType<"
                    << instanceName.toLocal8Bit().data()
                    << ">();";
            exit(1);
        }
    }
    return 0;
}

bool ControllerPrivate::superIsClassName(const QMetaObject *super, const QByteArray &className)
{
    if (super) {
        if (super->className() == className) {
            return true;
        }
        return superIsClassName(super->superClass(), className);
    }
    return false;
}
