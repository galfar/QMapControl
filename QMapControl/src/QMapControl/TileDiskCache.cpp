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
#include "TileDiskCache.hpp"
#include "qmapcontrol_global.h"
#include <QDateTime>
#include <QDir>

namespace qmapcontrol
{

void TileDiskCache::clear()
{
    QMutexLocker locker(&m_diskCacheMutex);
    m_networkDiskCache.clear();
}

bool TileDiskCache::setStorageDirectory(const QString& dir)
{
    QDir qdir(dir);

    if (qdir.mkpath(qdir.absolutePath()))
    {
        m_networkDiskCache.setCacheDirectory(qdir.absolutePath());
        m_valid = true;
        return true;
    }

    return false;
}

bool TileDiskCache::isValid() const
{
    return m_valid;
}

qint64 TileDiskCache::maximumSizeBytes() const
{
    return m_networkDiskCache.maximumCacheSize();
}

void TileDiskCache::setMaximumSizeBytes(const qint64& size)
{
    m_networkDiskCache.setMaximumCacheSize(size);
}

static QUrl tileToUrl(const TileDescriptor& coord)
{
    QString ret;
    QTextStream(&ret) << coord.sourceId << coord.x << "/" << coord.y << "/" << coord.zoom;
    return QUrl(ret);
}

QByteArray TileDiskCache::getTile(const TileDescriptor& coord) const
{
    QByteArray ret;
    QMutexLocker locker(&m_diskCacheMutex);
    QIODevice* device = m_networkDiskCache.data(tileToUrl(coord));

    DBG_SLEEP(5);
    if (device)
    {
        ret = device->readAll();
        device->close();
        delete device;
    }

    return ret;
}

bool TileDiskCache::insertTile(const TileDescriptor& coord, const QByteArray& tileData)
{
    QNetworkCacheMetaData meta;
    QMutexLocker locker(&m_diskCacheMutex);
    meta.setUrl(tileToUrl(coord));
    meta.setLastModified(QDateTime::currentDateTime());
    auto device = m_networkDiskCache.prepare(meta);

    if (device == nullptr)
    {
        return false;
    }

    DBG_SLEEP(10);
    if (device->write(tileData) < 0)
    {
        return false;
    }

    m_networkDiskCache.insert(device);
    return true;
}

}
