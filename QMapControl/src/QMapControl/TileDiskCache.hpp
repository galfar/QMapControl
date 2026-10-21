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

#include "ITileDiskCache.hpp"
#include "QNetworkDiskCache"
#include <QMutex>

namespace qmapcontrol {

class TileDiskCache : public ITileDiskCache
{
    Q_OBJECT
    Q_DISABLE_COPY(TileDiskCache)

public:

    explicit TileDiskCache(QObject* parent = nullptr) : ITileDiskCache(parent){}
    ~TileDiskCache() override = default;

    bool setStorageDirectory(const QString& dir);

    // ITileDiskCache
    void clear() override;
    qint64 maximumSizeBytes() const override;
    void setMaximumSizeBytes(const qint64& size) override;
    QByteArray getTile(const TileDescriptor& coords) const override;
    bool insertTile(const TileDescriptor& coord, const QByteArray& tileData) override;
    bool isValid() const override;

private:
    bool m_valid{false};
    mutable QMutex m_diskCacheMutex;
    mutable QNetworkDiskCache m_networkDiskCache{this};
};

}
