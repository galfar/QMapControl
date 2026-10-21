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

#include "Projection.h"

// Local includes.
#include "ProjectionEquirectangular.h"
#include "ProjectionSphericalMercator.h"
#include "ProjectionWorldMercator.h"

// STL includes.
#include <memory>

namespace qmapcontrol
{
    namespace
    {
        /// Singleton instance of Projection Interface.
        std::unique_ptr<Projection> m_instance = nullptr;
    }

    Projection& projection::get()
    {
        // Does the singleton instance exist?
        if (m_instance == nullptr)
        {
            // Create a default instance (Spherical Mercator).
            set(kDefaultEpsg);
        }

        // Return the reference to the instance object.
        return *(m_instance.get());
    }

    void projection::set(const EPSG type)
    {
        m_instance.reset(create(type));
    }

    Projection* projection::create(const EPSG type)
    {
        switch (type) {
        case EPSG::Equirectangular:
            return new ProjectionEquirectangular();
        case EPSG::SphericalMercator:
            return new ProjectionSphericalMercator();
        case EPSG::WorldMercator:
            return new ProjectionWorldMercator();
        default:
            qFatal("Unsuported EPSG: %d", static_cast<int>(type));
            Q_UNREACHABLE();
        }
    }

    int Projection::ipow2(int exponent) const {
        if (exponent >= 31) {
            return -1;
        }
        return 1 << exponent;
    }
}
