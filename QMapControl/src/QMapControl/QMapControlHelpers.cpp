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

#include "QMapControlHelpers.h"

// Qt includes.
#include <QTimer>
#include <QDebug>

// Local includes.
#include "Geometry.h"
#include "GeometryPoint.h"
#include "Projection.h"
#include "QMapControl.h"

namespace qmapcontrol
{
    AnimationHelper::AnimationHelper(QMapControl* mapControl)
        : QObject(mapControl)
        , m_mapControl(mapControl)
    {
    }

    void AnimationHelper::setMapFocusPointAnimated(const PointWorldCoord& coordinate, const int steps, const std::chrono::milliseconds& step_interval)
    {
        // Is animation already taking place?
        if (m_animated_mutex.tryLock())
        {
            // Set the new map focus point to scroll to.
            m_animated_map_focus_point = coordinate;

            // Set the number of steps it should take.
            m_animated_steps = steps;

            // Set the interval between each step.
            m_animated_interval = step_interval;

            // Set the animation tick to start.
            QTimer::singleShot(int(m_animated_interval.count()), this, &AnimationHelper::animatedTick);
        }
        else
        {
            // Warn that we cannot animate as we are already mid-animating.
            qDebug() << "Unable to animate to new map focus, already animating!";
        }
    }

    void AnimationHelper::animatedTick()
    {
        // Do we still have steps to complete?
        if (m_animated_steps > 0)
        {
            const PointWorldCoord current_focus_point = m_mapControl->mapFocusPointCoord();
            const int current_zoom = m_mapControl->getCurrentZoom();

            // Calculate the delta between the current map focus and the target animated map focus point.
            const PointWorldPx start_px(projection::get().toPointWorldPx(current_focus_point, current_zoom));
            const PointWorldPx dest_px(projection::get().toPointWorldPx(m_animated_map_focus_point, current_zoom));
            const PointPx delta_px(dest_px - start_px);

            // Scroll to the next point in the step.
            m_mapControl->scrollView(delta_px / m_animated_steps);

            // Reduce the number of steps left.
            m_animated_steps--;

            // Schedule the next animation tick.
            QTimer::singleShot(int(m_animated_interval.count()), this, &AnimationHelper::animatedTick);
        }
        else
        {
            // We have finished our animation, unlock the mutex.
            m_animated_mutex.unlock();
        }
    }

    FollowGeometryHelper::FollowGeometryHelper(QMapControl* mapControl)
        : QObject(mapControl)
        , m_mapControl(mapControl)
    {
    }

    void FollowGeometryHelper::followGeometry(const Geometry* geometry)
    {
        // Check the geometry is valid.
        if (geometry != nullptr)
        {
            // Remove any previous geometry following.
            stopFollowingGeometry();

            // Connect the position changed of the geometry to the controller.
            m_following_geometry = QObject::connect(geometry, &Geometry::positionChanged,
                                                    this, &FollowGeometryHelper::geometryPositionChanged);
        }
    }

    void FollowGeometryHelper::stopFollowingGeometry()
    {
        // Disconnect the signal/slot updates.
        QObject::disconnect(m_following_geometry);
    }

    void FollowGeometryHelper::geometryPositionChanged(const Geometry* geometry)
    {
        // Is it a point geometry?
        if (geometry->geometryType() == Geometry::GeometryType::GeometryPoint)
        {
            const PointWorldCoord map_focus_coord = m_mapControl->mapFocusPointCoord();
            const int current_zoom = m_mapControl->getCurrentZoom();

            // Calculate the delta between the current map focus and the new geometry position.
            const PointWorldPx start_px(projection::get().toPointWorldPx(map_focus_coord, current_zoom));
            const PointWorldPx dest_px(projection::get().toPointWorldPx(static_cast<const GeometryPoint*>(geometry)->coord(), current_zoom));
            const PointPx delta_px(dest_px - start_px);

            // Scroll the view
            m_mapControl->scrollView(delta_px);
        }
    }
}
