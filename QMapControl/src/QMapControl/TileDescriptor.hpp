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

#include <QString>
#include <QByteArray>
#include <QMetaType>

namespace qmapcontrol {


/*! Unique identifier of a tile */
struct TileDescriptor {
    QString sourceId;
    int x, y, zoom;

    QString toString() const
    {
        return QStringLiteral("%1 xyz:%2,%3,%4").arg(sourceId).arg(x).arg(y).arg(zoom);
    }

    QByteArray key() const
    {
        return QStringLiteral("%1/%2/%3/%4").arg(sourceId).arg(x).arg(y).arg(zoom).toUtf8();
    }
};

}//qmapcontrol

Q_DECLARE_METATYPE(qmapcontrol::TileDescriptor)
