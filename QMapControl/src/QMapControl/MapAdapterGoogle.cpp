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

#include "MapAdapterGoogle.h"

namespace qmapcontrol
{
    constexpr char kGoogleMapUrlFormat[] = "http://mt.google.com/vt/hl=en&x=%x&y=%y&z=%zoom&lyrs=";

    MapAdapterGoogle::MapAdapterGoogle(const MapAdapterGoogle::GoogleLayerType layer_type,
                                       QObject* parent)
        : MapAdapterTile(QUrl(kGoogleMapUrlFormat + layerTypeToString(layer_type)), { projection::EPSG::SphericalMercator },
                         0, 19, 0, false, parent)
    {
    }

    QString MapAdapterGoogle::layerTypeToString(const MapAdapterGoogle::GoogleLayerType layer_type)
    {
        // Convert the enum to a 1-character representation.
        switch (layer_type)
        {
            case GoogleLayerType::SATELLITE_ONLY:
                return "s";
            case GoogleLayerType::TERRAIN_ONLY:
                return "t";
            case GoogleLayerType::TERRAIN:
                return "p";
            case GoogleLayerType::ROADS_ONLY:
                return "h";
            case GoogleLayerType::HYBRID:
                return "y";
            case GoogleLayerType::RASTER:
                return "r";
            case GoogleLayerType::MAPS:
            default:
                return "m";
        }
    }    
}
