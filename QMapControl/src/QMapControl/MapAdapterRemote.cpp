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

#include "MapAdapterRemote.h"
#include "ImageManager.h"

// STL includes.
#include <cmath>
#include <QRegularExpression>

namespace qmapcontrol
{
    static QString quadKey(int zoom, int x, int y) {
        QString qk;
        qk.reserve(zoom);
        int tx = x;
        int ty = y;
        for (int i = zoom; i > 0; --i) {
            int digit = 0;
            const int mask = 1 << (i - 1);
            if (tx & mask) {
                digit += 1;
            } 
            if (ty & mask) {
                digit += 2;
            } 
            qk.append(QChar('0' + digit));
        }
        if (qk.isEmpty()) {
            qk.append(QChar('0'));
        }
        return qk;
    }


    MapAdapterRemote::MapAdapterRemote(const QUrl& base_url,
                                       const std::set<projection::EPSG>& epsg_projections,
                                       const int adapter_zoom_minimum,
                                       const int adapter_zoom_maximum,
                                       const bool invert_y)
        : MapAdapter(epsg_projections, adapter_zoom_minimum, adapter_zoom_maximum),
          m_base_url(base_url), m_invert_y(invert_y), m_sourceId(m_base_url.host())
    {
        m_tileUpscaling = true;
    }

    MapAdapterRemote::MapAdapterRemote(const QUrl& base_url,
                                       const QString& sourceId)
        : MapAdapterRemote(base_url, { projection::EPSG::SphericalMercator })
    {
        m_sourceId = sourceId;
    }

    QUrl MapAdapterRemote::getBaseUrl() const
    {
        // Return the url.
        return m_base_url;
    }

    void MapAdapterRemote::setBaseUrl(const QUrl& base_url)
    {
        // Set the base url.
        m_base_url = base_url;
    }

    QPixmap MapAdapterRemote::getTileImageInternal(const int x, const int y, const int controller_zoom, bool prefetchOnly) const
    {
        int maxSourceZoomLevelForUpscale = (m_tileUpscaling ? m_adapter_zoom_maximum : 0);
        auto generator = [this](int x, int y, int zoom)
        {
            return tileQuery(x, y, zoom);
        };

        return ImageManager::get().getImage(m_sourceId,
                                            x, y, toAdapterZoom(controller_zoom),
                                            generator,
                                            prefetchOnly, maxSourceZoomLevelForUpscale);
    }

    QUrl MapAdapterRemote::tileQuery(const int x, const int y, const int controller_zoom) const
    {
        // Capture inital y-axis tile request.
        int y_axis(y);
        // Do we need to invert the y-axis?
        if (m_invert_y)
        {
            // Inverse-y required.
            y_axis = projection::get().tilesY(controller_zoom - 1) - 1 - y;
        }
        // add support for serverparts (multiple urls for reaching tiles)
        QUrl url = selectServerPart(x, y, controller_zoom);
        if (!url.isValid()) {
            url = getBaseUrl();
        }

        QString urlString = url.toString();
        const bool isQuadKey = urlString.contains(QStringLiteral("%25q"), Qt::CaseInsensitive);
        if (isQuadKey) {
            QString qk = quadKey(controller_zoom, x, y_axis);
            urlString.replace(QStringLiteral("%25q"), qk, Qt::CaseInsensitive);
        } else {
            // Return a modified url with the %x, %y and %zoom values replaced.
            /// @note QUrl converts % into %25, so we replace %25x, %25y and %25zoom instead.
            urlString.replace(QStringLiteral("%25x"), QString::number(x), Qt::CaseInsensitive);
            urlString.replace(QStringLiteral("%25y"), QString::number(y_axis), Qt::CaseInsensitive);
            urlString.replace(QStringLiteral("%25zoom"), QString::number(controller_zoom), Qt::CaseInsensitive);
        }
        return QUrl(urlString);
    }

    void MapAdapterRemote::setServerParts(const QList<QUrl>& parts) {
        m_serverParts = parts;
    }

    QUrl MapAdapterRemote::selectServerPart(int x, int y, int zoom) const {
        if (m_serverParts.isEmpty()) {
            return QUrl();
        }

        const int idx = (x + y + zoom) % m_serverParts.count();
        return m_serverParts.at(idx);
    }
}


