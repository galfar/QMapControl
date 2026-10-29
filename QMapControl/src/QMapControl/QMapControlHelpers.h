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
#include <QMetaObject>
#include <QMutex>
#include <QObject>

// STL includes.
#include <chrono>

// Local includes.
#include "Point.h"

namespace qmapcontrol
{
    class Geometry;
    class QMapControl;

    class AnimationHelper : public QObject {
    Q_OBJECT
    public:
        explicit AnimationHelper(QMapControl* mapControl);

        AnimationHelper(const AnimationHelper&) = delete;
        AnimationHelper operator=(const AnimationHelper&) = delete;

        /*!
         * Smoothly moves the center of the view to the given coordinate.
         * @param coordinate The coordinate which the center of the view should moved to.
         * @param steps The number of steps to use in the animation.
         * @param step_interval The number of millseconds to wait between each step.
         */
        void setMapFocusPointAnimated(const PointWorldCoord& coordinate, const int steps = 25, const std::chrono::milliseconds& step_interval = std::chrono::milliseconds(50));

    private slots:
        /*!
         * Called during the animation loop to process the next step.
         */
        void animatedTick();

    private:
        /// Map Control for animation
        QMapControl* m_mapControl;

        /// Mutex to protect the animation loop.
        QMutex m_animated_mutex;

        /// Animation map focus point target.
        PointWorldCoord m_animated_map_focus_point;

        /// Number of animation steps remaining.
        int m_animated_steps;

        /// The number of millseconds to wait between each step.
        std::chrono::milliseconds m_animated_interval;
    };

    class FollowGeometryHelper : public QObject {
    Q_OBJECT
    public:
        explicit FollowGeometryHelper(QMapControl* mapControl);

        FollowGeometryHelper(const FollowGeometryHelper&) = delete;
        FollowGeometryHelper operator=(const FollowGeometryHelper&) = delete;

        /*!
         * Keeps the center of the map on the Geometry, even when it moves.
         * To stop following the geometry, call stopFollowing().
         * @param geometry The Geometry which should stay centered.
         */
        void followGeometry(const Geometry* geometry);

        /*!
         * Stop following a Geometry.
         */
        void stopFollowingGeometry();

    private slots:
        // Geometry management.
        /*!
         * Called when a geometry changes its position.
         * @param geometry The geometry that change position.
         */
        void geometryPositionChanged(const Geometry* geometry);

    private:
        /// Map Control for following
        QMapControl* m_mapControl;

        /// The current following geometry signal/slot connection.
        QMetaObject::Connection m_following_geometry;
    };
}
