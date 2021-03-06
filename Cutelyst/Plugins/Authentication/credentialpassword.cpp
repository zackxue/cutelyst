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

#include "credentialpassword_p.h"
#include "../authenticationrealm.h"

#include <QLoggingCategory>
#include <QMessageAuthenticationCode>
#include <QUuid>
#include <QFile>

using namespace Cutelyst;

Q_LOGGING_CATEGORY(C_CREDENTIALPASSWORD, "cutelyst.plugin.credentialpassword")

CredentialPassword::CredentialPassword(QObject *parent) : AuthenticationCredential(parent)
  , d_ptr(new CredentialPasswordPrivate)
{

}

CredentialPassword::~CredentialPassword()
{
    delete d_ptr;
}

AuthenticationUser CredentialPassword::authenticate(Context *c, AuthenticationRealm *realm, const CStringHash &authinfo)
{
    Q_D(CredentialPassword);
    AuthenticationUser user = realm->findUser(c, authinfo);
    if (!user.isNull()) {
        if (d->checkPassword(user, authinfo)) {
            return user;
        }
        qCDebug(C_CREDENTIALPASSWORD) << "Password didn't match";
    } else {
        qCDebug(C_CREDENTIALPASSWORD) << "Unable to locate a user matching user info provided in realm";
    }
    return AuthenticationUser();
}

QString CredentialPassword::passwordField() const
{
    Q_D(const CredentialPassword);
    return d->passwordField;
}

void CredentialPassword::setPasswordField(const QString &fieldName)
{
    Q_D(CredentialPassword);
    d->passwordField = fieldName;
}

CredentialPassword::Type CredentialPassword::passwordType() const
{
    Q_D(const CredentialPassword);
    return d->passwordType;
}

void CredentialPassword::setPasswordType(CredentialPassword::Type type)
{
    Q_D(CredentialPassword);
    d->passwordType = type;
}

QString CredentialPassword::passwordPreSalt() const
{
    Q_D(const CredentialPassword);
    return d->passwordPreSalt;
}

void CredentialPassword::setPasswordPreSalt(const QString &passwordPreSalt)
{
    Q_D(CredentialPassword);
    d->passwordPreSalt = passwordPreSalt;
}

QString CredentialPassword::passwordPostSalt() const
{
    Q_D(const CredentialPassword);
    return d->passwordPostSalt;
}

void CredentialPassword::setPasswordPostSalt(const QString &passwordPostSalt)
{
    Q_D(CredentialPassword);
    d->passwordPostSalt = passwordPostSalt;
}

// To avoid timming attack
bool slowEquals(const QByteArray &a, const QByteArray &b)
{
    int diff = a.size() ^ b.size();
    for(int i = 0; i < a.size() && i < b.size(); i++) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}

#define HASH_SECTIONS 4
#define HASH_ALGORITHM_INDEX 0
#define HASH_ITERATION_INDEX 1
#define HASH_SALT_INDEX 2
#define HASH_PBKDF2_INDEX 3
bool CredentialPassword::validatePassword(const QByteArray &password, const QByteArray &correctHash)
{
    QByteArrayList params = correctHash.split(':');
    if (params.size() < HASH_SECTIONS) {
        return false;
    }

    int method = CredentialPasswordPrivate::cryptoStrToEnum(params.at(HASH_ALGORITHM_INDEX));
    if (method == -1) {
        return false;
    }

    QByteArray pbkdf2Hash = QByteArray::fromBase64(params.at(HASH_PBKDF2_INDEX));
    return slowEquals(
                pbkdf2Hash,
                pbkdf2(
                    static_cast<QCryptographicHash::Algorithm>(method),
                    password,
                    params.at(HASH_SALT_INDEX),
                    params.at(HASH_ITERATION_INDEX).toInt(),
                    pbkdf2Hash.length()
                    )
                );
}

QByteArray CredentialPassword::createPassword(const QByteArray &password, QCryptographicHash::Algorithm method, int iterations, int saltByteSize, int hashByteSize)
{
    QByteArray salt;
    QFile random("/dev/urandom");
    if (!random.open(QIODevice::ReadOnly)) {
        salt = QUuid::createUuid().toByteArray().toBase64();
    } else {
        salt = random.read(saltByteSize).toBase64();
    }

    const QByteArray &methodStr = CredentialPasswordPrivate::cryptoEnumToStr(method);
    return methodStr + ':' + QByteArray::number(iterations) + ':' + salt + ':' +
            pbkdf2(
                method,
                password,
                salt,
                iterations,
                hashByteSize
                ).toBase64();
}

// TODO https://crackstation.net/hashing-security.htm
// shows a different Algorithm that seems a bit simpler
// this one does passes the RFC6070 tests
// https://www.ietf.org/rfc/rfc6070.txt
QByteArray CredentialPassword::pbkdf2(QCryptographicHash::Algorithm method, const QByteArray &password, const QByteArray &salt, int rounds, int keyLength)
{
    QByteArray key;

    if (rounds <= 0 || keyLength <= 0) {
        qCCritical(C_CREDENTIALPASSWORD, "PBKDF2 ERROR: Invalid parameters.");
        return QByteArray();
    }

    if (salt.size() == 0 || salt.size() > std::numeric_limits<int>::max() - 4) {
        return QByteArray();
    }

    QByteArray asalt = salt;
    asalt.resize(salt.size() + 4);

    for (int count = 1, remainingBytes = keyLength; remainingBytes > 0; ++count) {
        asalt[salt.size() + 0] = static_cast<char>((count >> 24) & 0xff);
        asalt[salt.size() + 1] = static_cast<char>((count >> 16) & 0xff);
        asalt[salt.size() + 2] = static_cast<char>((count >> 8) & 0xff);
        asalt[salt.size() + 3] = static_cast<char>(count & 0xff);
        QByteArray d1 = QMessageAuthenticationCode::hash(asalt, password, method);
        QByteArray obuf = d1;

        for (int i = 1; i < rounds; ++i) {
            d1 = QMessageAuthenticationCode::hash(d1, password, method);
            for (int j = 0; j < obuf.size(); ++j)
                obuf[j] = obuf[j] ^ d1[j];
        }

        key = key.append(obuf);
        remainingBytes -= obuf.size();

        d1.fill('\0');
        obuf.fill('\0');
    }

    asalt.fill('\0');

    return key.mid(0, keyLength);
}

QByteArray CredentialPassword::hmac(QCryptographicHash::Algorithm method, QByteArray key, const QByteArray &message)
{
    const int blocksize = 64;
    if (key.length() > blocksize) {
        return QCryptographicHash::hash(key, method);
    }

    while (key.length() < blocksize) {
        key.append('\0');
    }

    QByteArray o_key_pad('\x5c', blocksize);
    o_key_pad.fill('\x5c', blocksize);

    QByteArray i_key_pad;
    i_key_pad.fill('\x36', blocksize);

    for (int i=0; i < blocksize; i++) {
        o_key_pad[i] = o_key_pad[i] ^ key[i];
        i_key_pad[i] = i_key_pad[i] ^ key[i];
    }

    return QCryptographicHash::hash(o_key_pad + QCryptographicHash::hash(i_key_pad + message, method),
                                    method);
}

bool CredentialPasswordPrivate::checkPassword(const AuthenticationUser &user, const CStringHash &authinfo)
{
    QString password = authinfo.value(passwordField);
    const QString &storedPassword = user.value(passwordField);

    if (passwordType == CredentialPassword::None) {
        qCDebug(C_CREDENTIALPASSWORD) << "CredentialPassword is set to ignore password check";
        return true;
    } else if (passwordType == CredentialPassword::Clear) {
        return storedPassword == password;
    } else if (passwordType == CredentialPassword::Hashed) {
        if (!passwordPreSalt.isNull()) {
            password.prepend(password);
        }

        if (!passwordPostSalt.isNull()) {
            password.append(password);
        }

        return CredentialPassword::validatePassword(password.toUtf8(), storedPassword.toUtf8());
    } else if (passwordType == CredentialPassword::SelfCheck) {
        return user.checkPassword(password);
    }

    return false;
}

QByteArray CredentialPasswordPrivate::cryptoEnumToStr(QCryptographicHash::Algorithm method)
{
    QByteArray hashmethod;

#ifndef QT_CRYPTOGRAPHICHASH_ONLY_SHA1
    if (method == QCryptographicHash::Md4) {
        hashmethod = QByteArrayLiteral("Md4");
    } else if (method == QCryptographicHash::Md5) {
        hashmethod = QByteArrayLiteral("Md5");
    }
#endif
    if (method == QCryptographicHash::Sha1) {
        hashmethod = QByteArrayLiteral("Sha1");
    }
#ifndef QT_CRYPTOGRAPHICHASH_ONLY_SHA1
    if (method == QCryptographicHash::Sha224) {
        hashmethod = QByteArrayLiteral("Sha224");
    } else if (method == QCryptographicHash::Sha256) {
        hashmethod = QByteArrayLiteral("Sha256");
    } else if (method == QCryptographicHash::Sha384) {
        hashmethod = QByteArrayLiteral("Sha384");
    } else if (method == QCryptographicHash::Sha512) {
        hashmethod = QByteArrayLiteral("Sha512");
    } else if (method == QCryptographicHash::Sha3_224) {
        hashmethod = QByteArrayLiteral("Sha3_224");
    } else if (method == QCryptographicHash::Sha3_256) {
        hashmethod = QByteArrayLiteral("Sha3_256");
    } else if (method == QCryptographicHash::Sha3_384) {
        hashmethod = QByteArrayLiteral("Sha3_384");
    } else if (method == QCryptographicHash::Sha3_512) {
        hashmethod = QByteArrayLiteral("Sha3_512");
    }
#endif

    return hashmethod;
}

int CredentialPasswordPrivate::cryptoStrToEnum(const QByteArray &hashMethod)
{
    QByteArray hashmethod = hashMethod;

    int method = -1;
#ifndef QT_CRYPTOGRAPHICHASH_ONLY_SHA1
    if (hashmethod == "Md4") {
        method = QCryptographicHash::Md4;
    } else if (hashmethod == "Md5") {
        method = QCryptographicHash::Md5;
    }
#endif
    if (hashmethod == "Sha1") {
        method = QCryptographicHash::Sha1;
    }
#ifndef QT_CRYPTOGRAPHICHASH_ONLY_SHA1
    if (hashmethod == "Sha224") {
        method = QCryptographicHash::Sha224;
    } else if (hashmethod == "Sha256") {
        method = QCryptographicHash::Sha256;
    } else if (hashmethod == "Sha384") {
        method = QCryptographicHash::Sha384;
    } else if (hashmethod == "Sha512") {
        method = QCryptographicHash::Sha512;
    } else if (hashmethod == "Sha3_224") {
        method = QCryptographicHash::Sha3_224;
    } else if (hashmethod == "Sha3_256") {
        method = QCryptographicHash::Sha3_256;
    } else if (hashmethod == "Sha3_384") {
        method = QCryptographicHash::Sha3_384;
    } else if (hashmethod == "Sha3_512") {
        method = QCryptographicHash::Sha3_512;
    }
#endif

    return method;
}
