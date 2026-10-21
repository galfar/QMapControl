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

#include "MapAdapter.h"
#include "ImageManager.h"

// STL includes.
#include <cmath>

namespace qmapcontrol
{
    MapAdapter::MapAdapter(const std::set<projection::EPSG>& epsg_projections,
                           const int adapter_zoom_minimum,
                           const int adapter_zoom_maximum)
        : m_adapter_zoom_minimum(adapter_zoom_minimum),
          m_adapter_zoom_maximum(adapter_zoom_maximum),
          m_tileUpscaling(false),
          m_epsg_projections(epsg_projections)
    {
    }

    bool MapAdapter::isTileValid(const int x, const int y, const int controller_zoom) const
    {
        // Default success.
        bool success(false);

        // Check if the projection is supported.
        if (m_epsg_projections.find(static_cast<projection::EPSG>(projection::get().epsg())) == m_epsg_projections.end())
        {
            // Projection not supported, fail!
            qWarning("Unsupported EPSG projection: %d", projection::get().epsg());
        }
        // Else check the zoom is supported
        else if ((toAdapterZoom(controller_zoom) < m_adapter_zoom_minimum)
                 || ((toAdapterZoom(controller_zoom) > m_adapter_zoom_maximum) && !m_tileUpscaling))
        {
            // Controller zoom is out of range, fail!
        }
        // Else, check x and y are between 0 and "max_tiles" - 1 inclusive (as 0 based indexed!)
        else if ((x >= 0) && (x < projection::get().tilesX(controller_zoom))
                 && (y >= 0) && (y < projection::get().tilesY(controller_zoom)))
        {
            // Success, tile is valid.
            success = true;
        }

        // Return success;
        return success;
    }

    int MapAdapter::toAdapterZoom(const int controller_zoom) const
    {
        return controller_zoom;
    }

    QPixmap MapAdapter::getTileImage(const int x, const int y, const int controller_zoom) const
    {
        return getTileImageInternal(x, y, controller_zoom, false);
    }

    void MapAdapter::prefetchTile(const int x, const int y, const int controller_zoom) const
    {
        ImageManager& imgManager = ImageManager::get();
        if (!imgManager.isTileImagePresent(sourceId(), x, y, controller_zoom)) {
            (void)getTileImageInternal(x, y, controller_zoom, true);
        }
    }
}
