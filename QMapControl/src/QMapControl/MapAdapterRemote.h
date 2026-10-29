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

// Qt includes.
#include <QUrl>
#include <QList>

// Local includes.
#include "qmapcontrol_global.h"
#include "MapAdapter.h"

namespace qmapcontrol
{
    /*!
     * MapAdapter for servers with image tiles.
     * Use this derived MapAdapter to display maps from Tiled Map servers (OpenStreetMap/Google/etc...).
     *
     * @author Kai Winter <kaiwinter@gmx.de>
     * @author Chris Stylianou <chris5287@gmail.com>
     */
    class QMAPCONTROL_EXPORT MapAdapterRemote : public MapAdapter
    {
    public:
        //! Constructor.
        /*!
         * This construct a MapAdapter for Tiled Map (Slippymap) services.
         * Sample of a correct initialization of a MapAdapter.
         * std::shared_ptr<MapAdapterTile> mat(std::make_shared<MapAdapterRemote>(QUrl("http://192.168.8.1/img/img_cache.php/%zoom/%x/%y.png"), ...));
         * The placeholders available are: %zoom, %x and %y.
         * @param base_url The base url of the map server.
         * @param epsg_projections The supported EPSG projections.
         * @param adapter_zoom_minimum The adapter's minimum zoom level available.
         * @param adapter_zoom_maximum The adapter'smaximum zoom level available.
         * @param invert_y Whether the y-axis tile needs to be inverted (ie: y-axis tiles start at bottom-left, instead of top-left).
         */

        MapAdapterRemote(const QUrl& base_url,
                         const std::set<projection::EPSG>& epsg_projections,
                         const int adapter_zoom_minimum = 0,
                         const int adapter_zoom_maximum = kDefaultMaxZoom,
                         const bool invert_y = false);

        MapAdapterRemote(const QUrl& base_url,
                         const QString& sourceId);

        //! Disable copy constructor.
        MapAdapterRemote(const MapAdapterRemote&) = delete;

        //! Disable copy assignment.
        MapAdapterRemote& operator=(const MapAdapterRemote&) = delete;

        //! Destructor.
        ~MapAdapterRemote() override = default;

        /*!
         * Get the base url.
         * @return the base url.
         */
        QUrl getBaseUrl() const;

        /*!
         * Change the base url post-initialisation.
         * @param base_url The new base url to set.
         */
        virtual void setBaseUrl(const QUrl& base_url);

        /*!
         * Generates the url required to fetch the image tile for the specified x, y and zoom.
         * @param x The x coordinate required.
         * @param y The y coordinate required.
         * @param controller_zoom The current controller zoom.
         * @return the generated url.
         */
        virtual QUrl tileQuery(const int x, const int y, const int controller_zoom) const;

        /*!
         * Source ID can be used to identify the tiles as coming from this adapter
         * in caching or other uses.
         */
        QString sourceId() const override { return m_sourceId; }

        /*!
         * Sets available server parts, e.g. { "a", "b", "c" }
         */
        void setServerParts(const  QList<QUrl>& parts);

        /*!
         * Selects serverpart url
         */
        QUrl selectServerPart(int x, int y, int zoom) const;

    private:
        /// The base url path of the map server.
        QUrl m_base_url;
        // List of server parts (optional)
        QList<QUrl> m_serverParts;

        /// Whether the y-axis tile needs to be inverted (ie: y-axis tiles start at bottom-left, instead of top-left).
        const bool m_invert_y;
    protected:
        QString m_sourceId;

        QPixmap getTileImageInternal(const int x, const int y, const int controller_zoom, bool prefetchOnly) const override;
    };
}
