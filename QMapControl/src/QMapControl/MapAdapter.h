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
#include <QPixmap>
#include <QString>

// STL includes.
#include <set>

// Local includes.
#include "qmapcontrol_global.h"
#include "Projection.h"

namespace qmapcontrol
{
    /*!
     * Used to fit map data sources into QMapControl.
     *
     * @author Kai Winter <kaiwinter@gmx.de>
     * @author Chris Stylianou <chris5287@gmail.com>
     */
    class QMAPCONTROL_EXPORT MapAdapter
    {
    public:
        //! Disable copy constructor.
        MapAdapter(const MapAdapter&) = delete;

        //! Disable copy assignment.
        MapAdapter& operator=(const MapAdapter&) = delete;

        //! Destructor.
        virtual ~MapAdapter() = default;

        /*!
         * These methods give map adapters a possibility to prepare for and cleanup after
         * the layer draws itself (using this adapater).
         */
        virtual bool prepareForDrawing(const RectWorldPx& view_rect_px, const int controller_zoom) {
            Q_UNUSED(view_rect_px); Q_UNUSED(controller_zoom); return true; }
        virtual void finishDrawing() { }

        /*!
         * Returns tile image for given XYZ tile coordinate. Tile can come from
         * remote or local source depending on given MapAdapter descentant.
         */
        QPixmap getTileImage(const int x, const int y, const int controller_zoom) const;

        /*!
         * Prefetches a tile that may be soon needed for display.
         */
        void prefetchTile(const int x, const int y, const int controller_zoom) const;

        /*!
         * Indicates whether a given x, y and controller zoom would provide a valid tile image.
         * @param x The x coordinate required.
         * @param y The y coordinate required.
         * @param controller_zoom The current controller zoom.
         * @return whether their would be a valid image tile.
         */
        virtual bool isTileValid(const int x, const int y, const int controller_zoom) const;

        /*!
         * Unique identifier of map adapter data source used for tile queries and caching.
         */
        virtual QString sourceId() const = 0;

        /*!
         * Set to true to enable upscaling of tile images when controller_zoom > adapter_max_zoom.
         */
        void setTileUpscaling(bool upscaling) { m_tileUpscaling = upscaling; }

        int zoomMaximum() const { return m_adapter_zoom_maximum; }
        int zoomMinimum() const { return m_adapter_zoom_minimum; }

    protected:
        /// The minimum adapter zoom level available.
        int m_adapter_zoom_minimum;
        /// The maximum adapter zoom level available.
        int m_adapter_zoom_maximum;
        /// upscaling of tile images when controller_zoom > adapter_max_zoom
        bool m_tileUpscaling;

        //! Constructor.
        /*!
         * This construct a Map Adapter.
         * @param epsg_projections The supported EPSG projections.
         * @param adapter_zoom_minimum The adapter's minimum zoom level available.
         * @param adapter_zoom_maximum The adapter'smaximum zoom level available.
         */
        MapAdapter(const std::set<projection::EPSG>& epsg_projections,
                   const int adapter_zoom_minimum,
                   const int adapter_zoom_maximum);

        /*!
         * Translates the controller zoom in the map adapters zoom.
         * Special adapters have a chance here to adjust the zoom level.
         * @param controller_zoom The controller's zoom.
         * @return the map adapters zoom.
         */
        virtual int toAdapterZoom(const int controller_zoom) const;

        virtual QPixmap getTileImageInternal(const int x, const int y, const int controller_zoom, bool prefetchOnly) const = 0;

    private:
        /// The supported EPSG projections.
        const std::set<projection::EPSG> m_epsg_projections;
    };
}
