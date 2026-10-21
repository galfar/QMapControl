/*
*
* This file is part of QMapControl,
* an open-source cross-platform map widget
*
* Copyright (C) 2007 - 2008 Kai Winter
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with QMapControl. If not, see <http://www.gnu.org/licenses/>.
*
* Contact e-mail: kaiwinter@gmx.de
* Program URL   : http://qmapcontrol.sourceforge.net/
*
*/
#pragma once

#include <QObject>

class QNetworkRequest;
class QNetworkReply;
class QNetworkProxy;
class QAuthenticator;

namespace qmapcontrol
{
    class INetworkAccessManager : public QObject
    {

    Q_OBJECT

    public:
        explicit INetworkAccessManager(QObject *parent = nullptr) : QObject(parent) {}
        ~INetworkAccessManager() override {}

        virtual void setProxy(const QNetworkProxy &proxy) = 0;
        virtual QNetworkReply* get(const QNetworkRequest &request) = 0;

        virtual void clearConnectionCache() {}

    signals:
        void proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator);
        void finished(QNetworkReply *reply);
    };
}
