#pragma once

// Qt includes.
#include <QtCore/QtGlobal>

/*!
 * @author Kai Winter <kaiwinter@gmx.de>
 * @author Chris Stylianou <chris5287@gmail.com>
 */

/*
#ifdef QMAPCONTROL_LIBRARY
    /// Defines that the specified function, variable or class should be exported.
    #define QMAPCONTROL_EXPORT Q_DECL_EXPORT
#else
    /// Defines that the specified function, variable or class should be imported.
    #define QMAPCONTROL_EXPORT Q_DECL_IMPORT
#endif
*/

/// Set to non-zero for debug output
#define QMAP_DEBUG 0
/// Set to non-zero multiplicator to simulate slow HW operation
/// Helps to uncover race conditions and performance related issues
#define QMAP_DEBUG_EMULATE_SLOW 0

#if (QMAP_DEBUG > 0)

#define QMAP_DBG() qDebug()
#define QMAP_INFO() qInfo()

#else

#define QMAP_DBG() if (0) qDebug()
#define QMAP_INFO() if (0) qInfo()

#endif

#if (QMAP_DEBUG_EMULATE_SLOW > 0)

#include <QThread>

/// insert artificial sleep times to emulate slow hardware
#define DBG_SLEEP(time) \
    do { \
        QMAP_DBG() << "Sleeping for: " << (time); \
        QThread::msleep((time)*(QMAP_DEBUG_EMULATE_SLOW)); \
    } while(0)

#else

#define DBG_SLEEP(time) do {} while(0)

#endif

// MSVC does not define M_PI in <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define QMAPCONTROL_EXPORT

constexpr int kDefaultMaxZoom = 21;
