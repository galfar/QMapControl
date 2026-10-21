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
#include "TileDescriptor.hpp"
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTextStream>

namespace qmapcontrol
{

class ITileDiskCache : public QObject
{
    Q_OBJECT

public:
    explicit ITileDiskCache(QObject *parent = nullptr) : QObject(parent) {}
    ~ITileDiskCache() override {}

    /*!
     * Clear all the data
     */
    virtual void clear() = 0;

    /*!
     * use for checking if disk cache object is currently valid (all param set)
     * @return Whether is valid or not.
     */
    virtual bool isValid() const = 0;

    /*!
     * Gets the maximum size on disk to be occupied
     * @return The size in bytes
     */
    virtual qint64 maximumSizeBytes() const = 0;

    /*!
     * Sets the maximum size on disk to be occupied
     * @param size The size in bytes
     */
    virtual void setMaximumSizeBytes(const qint64& size) = 0;

    /*!
     * Gets image for specified tile
     * @param coords The tile coordinate.
     * @return Tile's data or empty if not found.
     */
    virtual QByteArray getTile(const TileDescriptor& coords) const = 0;

    /*!
     * Inserts the specified tile
     * @param coords coordinates of the tile to insert.
     * @param tileData tile's data.
     * @return Whether the tile has been inserted into the container.
     */
    virtual bool insertTile(const TileDescriptor& coords, const QByteArray& tileData) = 0;
};

}
