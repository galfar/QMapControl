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

// Local includes.
#include "ImageManager.h"
#include "Projection.h"
#include "ITileDiskCache.hpp"

// Qt includes.
#include <QPainter>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

//STL includes.
#include <cmath>
#include <memory>

namespace qmapcontrol
{
    struct ImageManager::TileRequest
    {
        TileDescriptor coord;
        RequestPurpose purpose;
    };
    const int kDefaultTileSizePx = 256;
    const int kDefaultPixmapCacheSizeMiB = 30;

    namespace
    {
        /// Singleton instance of Image Manager.
        std::unique_ptr<ImageManager> m_instance = nullptr;

        // this is a FAKE instance counter as we have only a singleton of QMapControl
        // but we have to govern the sequence, that only the really last one destructs the singleton
        int m_instanceCounter = 0;
    }

    ImageManager& ImageManager::create()
    {
        QMAP_DBG() << "instance count went from " << m_instanceCounter
                   << " to " << (m_instanceCounter + 1);
        m_instanceCounter++;

        // Does the singleton instance exist?
        if (m_instance == nullptr)
        {
            // Create a default instance
            m_instance.reset(new ImageManager(kDefaultTileSizePx));
        }

        Q_ASSERT(m_instance);
        // Return the reference to the instance object.
        return *(m_instance.get());
    }

    ImageManager& ImageManager::get()
    {
        Q_ASSERT(m_instance);
        // Return the reference to the instance object.
        return *(m_instance.get());
    }

    void ImageManager::destroy()
    {
        QMAP_DBG() << "instance count went from " << m_instanceCounter
                   << " to " << (m_instanceCounter - 1);

        // only the last destruction will actually reset the singleton
        m_instanceCounter--;

        if (m_instanceCounter > 0) {
            return;
        }

        QMAP_DBG() << "Destroying ImageManager";
        // Ensure the singleton instance is destroyed.
        m_instance.reset(nullptr);
    }

    ImageManager::ImageManager(const int tile_size_px, QObject* parent)
        : QObject(parent),
          m_tile_size_px(tile_size_px),
          m_networkAccessManager(nullptr),
          m_diskCache(nullptr),
          m_diskCachePolicy(CachePolicy::AlwaysCache),
          m_diskCacheReadOnly(false)
    {
        QMAP_DBG() << "Spawning new ImageManager";
        qRegisterMetaType<RequestPurpose>("RequestPurpose");
        qRegisterMetaType<TileDescriptor>("TileDescriptor");
        qRegisterMetaType<Failure>("Failure");

        setMemoryCacheCapacity(kDefaultPixmapCacheSizeMiB);
        // Setup a loading/empty pixmaps
        setupPlaceholderPixmaps();
        m_threadPool = QThreadPool::globalInstance();
    }

    ImageManager::~ImageManager() {
        if (m_instance!=nullptr) {
            qWarning("ImageManager has not destroyed in ordinary fashion.");
        }
        // suspend any incoming requests from being processed (return empty pixmaps)
        m_diskCachePolicy = CachePolicy::AlwaysCache;
        if (m_networkManager)
        {
            m_networkManager->disconnect(this);
            disconnect(m_networkManager);
            m_networkManager = nullptr;
        }
        m_diskCache = nullptr;
        clearMemoryCache();
        if (m_threadPool) {
            if (!m_threadPool->waitForDone(2000)) {
                qWarning() << "Background tasks did not finish in time";
            }
        }
    }

    void ImageManager::enableNetworkManager(bool enable) {
        if (enable)
        {
            if (!m_networkManager)
            {
                QMAP_DBG() << "Spawning Network Manager";

                if (!m_networkAccessManager) {
                    qCritical() << "NetworkAccessManager was not assigned";
                }
                m_networkManager = new NetworkManager(m_networkAccessManager, this);
                m_networkManager->setProxy(m_proxy);
                // Connect signal/slot for image downloads.
                // QueuedConnection because of the m_requestsLock
                connect(this, &ImageManager::downloadTile, m_networkManager, &NetworkManager::downloadTile, Qt::QueuedConnection);
                connect(m_networkManager, &NetworkManager::tileDownloaded, this, &ImageManager::handleTileDownloaded);
                connect(m_networkManager, &NetworkManager::tileDownloadFailed, this, &ImageManager::handleTileDownloadFailed);
                connect(m_networkManager, &NetworkManager::downloadingInProgress, this, &ImageManager::downloadingInProgress);
                connect(m_networkManager, &NetworkManager::downloadingFinished, this, &ImageManager::downloadingFinished);
            }
        }
        else
        {
            if (m_networkManager)
            {
                QMAP_DBG() << "Destroying Network Manager";
                abortLoading();
                m_networkManager->disconnect(this);
                disconnect(m_networkManager);
                m_networkManager->deleteLater();
                m_networkManager = nullptr;
            }
        }
    }

    int ImageManager::tileSizePx() const
    {
        // Return the tiles size in pixels.
        return m_tile_size_px;
    }

    void ImageManager::setTileSizePx(const int tile_size_px)
    {
        // Set the new tile size.
        m_tile_size_px = tile_size_px;

        // Create a new loading pixmap.
        setupPlaceholderPixmaps();
    }

    void ImageManager::setProxy(const QNetworkProxy& proxy)
    {
        m_proxy = proxy;
        if (m_networkManager)
        {
            // Set the proxy on the network manager.
            m_networkManager->setProxy(proxy);
        }
    }

    void ImageManager::setThreadPool(QThreadPool* pool)
    {
        m_threadPool = pool;
        m_threadPool->setParent(this);
    }

    void ImageManager::setLoadingPixmap(const QPixmap &pixmap)
    {
        m_pixmapLoading = pixmap;
    }

    qint64 ImageManager::loadingPixmapCacheKey()
    {
        return m_pixmapLoading.cacheKey();
    }

    void ImageManager::setEmptyPixmap(const QPixmap &pixmap)
    {
        m_pixmapEmpty = pixmap;
    }

    qint64 ImageManager::emptyPixmapCacheKey()
    {
        return m_pixmapEmpty.cacheKey();
    }

    void ImageManager::setupPlaceholderPixmaps()
    {
        m_pixmapEmpty = QPixmap(m_tile_size_px, m_tile_size_px);
        m_pixmapEmpty.fill(Qt::transparent);

        // Create a new pixmap.
        m_pixmapLoading = QPixmap(m_pixmapEmpty);

        // Add a pattern.
        QPainter painter(&m_pixmapLoading);
        QBrush brush(Qt::lightGray, Qt::Dense5Pattern);
        painter.fillRect(m_pixmapLoading.rect(), brush);

        // Add "LOADING..." text.
        painter.setPen(Qt::black);
        painter.drawText(m_pixmapLoading.rect(), Qt::AlignCenter, "LOADING...");
    }

    bool ImageManager::configureDiskCache(ITileDiskCache* diskCache)
    {
        if (diskCache && !diskCache->isValid())
        {
            return false;
        }

        if (m_diskCache)
        {
            // Don't do this (leave it to termination)
            // it's currently unsafe, we would have to block till threadpool is finished
            //m_diskCache->deleteLater();
        }
        m_diskCache = diskCache;
        if (diskCache)
        {
            diskCache->setParent(this);
        }
        return true;
    }

    void ImageManager::clearDiskCache()
    {
        if (m_diskCache)
        {
            m_diskCache->clear();
        }
    }

    void ImageManager::abortLoading()
    {
        // Abort any remaining network manager downloads.
        if (m_networkManager)
        {
            m_networkManager->abortDownloads();
        }
    }

    int ImageManager::downloadQueueSize() const
    {
        // Return the number of pending requests
        QReadLocker lock(&m_requestsLock);
        return m_requestedTiles.count();
    }

    QPixmap ImageManager::getImage(const QString& sourceId,
                                   int x, int y, int zoom,
                                   const UrlGenerator& urlGenerator,
                                   bool prefetchOnly, int maxSourceZoomForUpscale)
    {
        // NOTE: Runs in a pooled thread during map rendering
        Q_ASSERT(urlGenerator);

        TileDescriptor tileCoord{sourceId, x, y, zoom};
        RequestPurpose purpose = (!prefetchOnly ? RequestPurpose::Display : RequestPurpose::Prefetch);
        QUrl url = urlGenerator( x, y, zoom);
        {
            QWriteLocker lock(&m_requestsLock);
            auto it = m_requestedTiles.find(url);
            if (it != m_requestedTiles.end())
            {
                // upgrade request
                if (it.value().purpose != purpose && it.value().purpose != RequestPurpose::Display)
                {
                    QMAP_DBG() << "Upgrading purpose of '" << tileCoord.toString()
                               << "' with URL " << url.toString()
                               << " " << purpose;

                    it.value().purpose = purpose;
                }
                else
                {
                    QMAP_DBG() << "Already requested: " << url.toString();
                }
                return m_pixmapLoading;
            }
        }

        const QByteArray tileKey = tileCoord.key();

        QPixmap pixmap;
        if (findTileInMemoryCache(tileKey, pixmap))
        {
            QMAP_DBG() << "Serving from mem-cache: " << url.toString();
            Q_ASSERT(!pixmap.isNull());
            // Image found in memory cache, use it
            return pixmap;
        }

        bool doUpscale = ((maxSourceZoomForUpscale > 0) && (zoom > maxSourceZoomForUpscale));
        if (doUpscale)
        {
            // Upscale is requested, top tile provider using given URL generator is passed in
            pixmap = upscaleTopTile(tileCoord, maxSourceZoomForUpscale,
                                    [this, sourceId, urlGenerator](int x, int y, int zoom)
                                    {
                                        return getImage(sourceId, x, y, zoom, urlGenerator, true, -1);
                                    });
            if (!pixmap.isNull())
            {
                insertTileToMemoryCache(tileKey, pixmap);
            }
            return pixmap;
        }

        // Consult cache if tile is already present
        if ((m_diskCachePolicy == CachePolicy::AlwaysCache)
                || (m_diskCachePolicy == CachePolicy::PreferCache))
        {
            if (m_diskCache != nullptr)
            {
                const QByteArray data = m_diskCache->getTile(tileCoord);

                if (!data.isEmpty())
                {
                    QPixmap pixmap = getImageFromByteArray(tileKey, data);
                    if (pixmap.isNull())
                    {
                        qWarning() << "Pixmap is empty for disk-cached '" << tileCoord.toString()
                                   << "', data size: " << data.size();
                        return m_pixmapEmpty;
                    }
                    QMAP_DBG() << "Hit disk cache for:" << tileCoord.toString();
                    return pixmap;
                }
            }

            // In offline mode just look in the caches, no downloads
            if (m_diskCachePolicy == CachePolicy::AlwaysCache)
            {
                return m_pixmapEmpty;
            }
        }

        if (m_networkManager)
        {
            // Emit that we need to download the image using the network manager.
            QMAP_DBG() << "Scheduling download of '" << tileCoord.toString()
                       << "' with URL " << url.toString()
                       << " " << purpose;
            {
                QWriteLocker locker(&m_requestsLock);
                m_requestedTiles.insert(url, TileRequest{tileCoord, purpose});
            }
            emit downloadTile(url);
            // Image not found, return "loading" image
            return m_pixmapLoading;
        }
        else
        {
            return m_pixmapEmpty;
        }
    }

    QPixmap ImageManager::getImage(const QString& sourceId,
                                   int x, int y, int zoom,
                                   const TileDataProvider& tileDataProvider,
                                   bool prefetchOnly, int maxSourceZoomForUpscale)
    {
        // NOTE: Runs in a pooled thread during map rendering
        Q_ASSERT(tileDataProvider);
        TileDescriptor tileCoord{sourceId, x, y, zoom};
        const QByteArray tileKey = tileCoord.key();

        // Prefetch was blocking and was disabled for gpkg
        // Prefetching itself is a good idea but implementaion would need something similar as the
        // online sources (asynchronous read/decode with queueing rather than direct blocking)
        if (prefetchOnly) {
            return QPixmap();
        }

        QPixmap pixmap;
        if (findTileInMemoryCache(tileKey, pixmap))
        {
            Q_ASSERT(!pixmap.isNull());
            // Image found in memory cache, use it
            return pixmap;
        }

        bool doUpscale = ((maxSourceZoomForUpscale > 0) && (zoom > maxSourceZoomForUpscale));
        if (doUpscale)
        {
            // Upscale is requested, top tile provider using given data provider is passed in
            pixmap = upscaleTopTile(tileCoord, maxSourceZoomForUpscale,
                                    [this, sourceId, tileDataProvider, prefetchOnly](int x, int y, int zoom)
                                    {
                                        // make the 'prefetchOnly' hardcoded 'true'
                                        // in the same way as for online sources, once the proper async. prefetch is implemented
                                        return getImage(sourceId, x, y, zoom, tileDataProvider, prefetchOnly, -1);
                                    });
            if (!pixmap.isNull())
            {
                insertTileToMemoryCache(tileKey, pixmap);
            }
            return pixmap;
        }

        // Ask external source for the tile data
        QByteArray tileData = tileDataProvider(x, y, zoom);
        if (!tileData.isEmpty())
        {
            pixmap = getImageFromByteArray(tileKey, tileData);

            if (pixmap.isNull())
            {
                qWarning() << "Pixmap is empty for '" << tileCoord.toString() << "', data size: " << tileData.size();
                return m_pixmapEmpty;
            }
            return pixmap;
        }

        return m_pixmapEmpty;
    }

    QPixmap ImageManager::upscaleTopTile(const TileDescriptor& tile, int maxSourceZoom,
                                         const TileImageProvider& topTileProvider)
    {
        DBG_SLEEP(10);

        // First we calculate tile coords for the top tile (tile at maxSourceZoom level).
        // Then copy and scale up the area covering the area of the requested tile.
        int zoomDiff = tile.zoom - maxSourceZoom;
        // Scale factor is used both for top tile lookup and subsequent stretched draw
        // from top tile to tile at desired zoom level. Valid for power of 2 zoom
        // levels. Note that tile matrix width differences between projections (Mercators vs
        // EPSG 4326) do not matter here.
        double scaleFactor = std::pow(2, zoomDiff);

        double topTileX = tile.x / scaleFactor;
        double topTileY = tile.y / scaleFactor;

        QPixmap topTile = topTileProvider(static_cast<int>(std::floor(topTileX)),
                                          static_cast<int>(std::floor(topTileY)),
                                          maxSourceZoom);

        if (topTile.isNull() || (topTile.cacheKey() == m_pixmapLoading.cacheKey()))
        {
            // Top tile is not available at the moment
            return m_pixmapEmpty;
        }

        QPixmap destTile = QPixmap(topTile.width(), topTile.height());

        double srcX = (topTileX - std::floor(topTileX)) * topTile.width();
        double srcY = (topTileY - std::floor(topTileY)) * topTile.height();

        // Move half a pixel so bilinear filtering handles "sub" tile edges nicely.
        // Also make sure the size is at least 1x1 pixels or drawPixmap fails.
        QRectF srcRect_px(std::max(0.0, srcX),
                        std::max(0.0, srcY),
                        std::max(1.0, topTile.width() / scaleFactor),
                        std::max(1.0, topTile.height() / scaleFactor));

        // Tiles may have transparent parts but these won't be properly
        // copied from top tile by QPixmap's draw calls regardless of
        // composition mode.
        // In this case we must pre-fill pixmap with transparent color
        // as pixmap data is unintialized garbage when created and this would show
        // in the transparent parts.

        // Ideally, we only fill when really (rarely) needed (fill may involve QImage conversion).
        // Fortunately, hasAlphaChannel() does report false for ordinary JPEG and opaque
        // PNG tiles (provided they're stored as 24bit in GPKG).
        bool hasAlpha = topTile.hasAlphaChannel();
        if (hasAlpha) {
            destTile.fill(Qt::transparent);
        }

        QPainter painter(&destTile);
        painter.drawPixmap(destTile.rect(), topTile, srcRect_px);

        return destTile;
    }

    QByteArray ImageManager::tileFromDiskCache(const QString& sourceId,
                                               int x, int y, int zoom) const
    {
        return tileFromDiskCache(TileDescriptor{sourceId, x, y, zoom});
    }

    QByteArray ImageManager::tileFromDiskCache(const TileDescriptor& tileCoord) const
    {
        if (m_diskCache != nullptr)
        {
            return m_diskCache->getTile(tileCoord);
        }

        return QByteArray();
    }

    QPixmap ImageManager::getImageFromByteArray(const QByteArray& tileKey, const QByteArray& data)
    {
        QPixmap pixmap;
        if (pixmap.loadFromData(data))
        {
            // Add decoded tile image to the pixmap cache
            insertTileToMemoryCache(tileKey, pixmap);
        }
        return pixmap;
    }

    bool ImageManager::cacheImageToDisk(const UrlGenerator& urlGenerator,
                                        const QString& sourceId,
                                        int x, int y, int zoom)
    {
        TileDescriptor tileCoord{sourceId, x, y, zoom};

        if (m_diskCachePolicy == CachePolicy::AlwaysCache || m_diskCachePolicy == CachePolicy::PreferCache)
        {
            // NOTE: this reads the tile from disk, it's safer but slow for large tile repositories.
            const QByteArray tileData = tileFromDiskCache(tileCoord);
            if (!tileData.isEmpty())
            {
                emit tileCached(tileCoord, true, tileData);
                return true;
            }
            if (m_diskCachePolicy == CachePolicy::AlwaysCache)
            {
                emit tileDownloadFailed(tileCoord, Failure::ContentDoesNotExist, RequestPurpose::Cache);
                return true;
            }
        }
        // Emit that we need to download the tile using the network manager.
        QUrl url = urlGenerator(x, y, zoom);
        {
            QWriteLocker locker(&m_requestsLock);
            if (!m_requestedTiles.contains(url))
            {
                m_requestedTiles.insert(url, TileRequest{tileCoord, RequestPurpose::Cache});
                emit downloadTile(url);
            }
        }
        return false;
    }

    void ImageManager::setDiskCachePolicy(CachePolicy policy)
    {
        if (policy != m_diskCachePolicy)
        {
            m_diskCachePolicy = policy;
            enableNetworkManager(m_diskCachePolicy != CachePolicy::AlwaysCache);
        }
    }

    void ImageManager::setNetworkManager(INetworkAccessManager* networkAccessManager) {
        // allow to set only once
        if (m_networkAccessManager) {
            qCritical() << "NetworkAccessManager was already assigned";
            return;
        }

        m_networkAccessManager = networkAccessManager;
    }

    void ImageManager::setDiskCacheReadOnly(bool readOnly) {
        m_diskCacheReadOnly = readOnly;
    }

    void ImageManager::decodeRequestedTileImage(const QByteArray& tileData,
                                                const TileDescriptor& coord,
                                                RequestPurpose purpose)
    {
        DBG_SLEEP(5);
        QPixmap pixmap = getImageFromByteArray(coord.key(), tileData);

        if (pixmap.isNull())
        {
            qWarning() << "Pixmap is empty for " << coord.toString() << ", data size: " << tileData.size();
        }
        if (purpose == RequestPurpose::Display)
        {
            // This tile was requested by map painter so let's notify it
            emit imageUpdated();
        }
    }

    void ImageManager::handleTileDownloaded(const QUrl& url,
                                            const QByteArray& tileData)
    {
        //retrieving the tile coordinates in the main thread since possibly two slave thread needs them
        TileRequest request;
        {
            QReadLocker locker(&m_requestsLock);
            if (!m_requestedTiles.contains(url))
            {
                return;
            }
            request = m_requestedTiles.value(url);
        }
        const TileDescriptor coord = request.coord;
        QMAP_DBG() << "ImageManager::handleTileDownloaded '" << url << "'" << request.purpose;

        // Insert the received tile into the disk cache
        if ((m_diskCache != nullptr) && (m_diskCachePolicy != CachePolicy::AlwaysNetwork))
        {
            if (!m_diskCacheReadOnly) {
                QtConcurrent::run(m_threadPool, [this, coord, tileData]()
                {
                    if (m_diskCache != nullptr)
                    {
                        if (!m_diskCache->insertTile(coord, tileData))
                        {
                            qWarning() << "Failed to store the tile in cache.";
                        }
                    }
                });
            }
            // Announce we have the stuff
            emit tileCached(coord, false, tileData);
        }

        if (request.purpose != RequestPurpose::Cache)
        {
            // Decodes the tile to pixmap and also puts it in memory cache
            QtConcurrent::run(m_threadPool, [this, url, request, coord, tileData]()
            {
                decodeRequestedTileImage(tileData, coord, request.purpose);
                // now it's time to remove request from the queue
                QWriteLocker locker(&m_requestsLock);
                (void)m_requestedTiles.remove(url);
            });
        }
        else
        {
            // now it's time to remove request from the queue
            QWriteLocker locker(&m_requestsLock);
            (void)m_requestedTiles.remove(url);
        }

        emit tileDownloadSucceeded(coord, request.purpose);
    }

    void ImageManager::handleTileDownloadFailed(const QUrl& url, Failure failure)
    {
        // NOTE: Runs in the main thread
        QMAP_DBG() << "ImageManager::handleTileDownloadFailed '" << url << "'";

        TileRequest request;
        {
            QWriteLocker locker(&m_requestsLock);
            if (!m_requestedTiles.contains(url))
            {
                return;
            }
            request = m_requestedTiles.take(url);
        }

        if ((request.purpose != RequestPurpose::Cache)
                && (m_diskCachePolicy == CachePolicy::PreferNetwork)
                && (failure != Failure::Cancelled))
        {
            // With PreferNetwork we look into the disk cache only when network request for a tile
            // fails. We're not interested in the exact reason (well apart from cancellation),
            // NetworkManager emits this signal as it sees fit (some failures are retried etc.).

            QtConcurrent::run(m_threadPool, [this, request]()
            {
                const TileDescriptor coord = request.coord;
                const QByteArray cachedTileData = tileFromDiskCache(coord);
                if (!cachedTileData.isEmpty())
                {
                    decodeRequestedTileImage(cachedTileData, coord, request.purpose);
                }
            });
        }

        emit tileDownloadFailed(request.coord, failure, request.purpose);
    }

    void ImageManager::setMemoryCacheCapacity(int capacityMiB)
    {
        m_memoryCache.setMaxCost(capacityMiB * 1024 * 1024);
    }

    void ImageManager::clearMemoryCache()
    {
        QWriteLocker locker(&m_memoryCacheLock);
        m_memoryCache.clear();
    }

    class QPixmapCacheEntry : public QPixmap
    {
    public:
        QPixmapCacheEntry(const QPixmap &pixmap) : QPixmap(pixmap) { }
    };

    bool ImageManager::isTileImagePresent(const TileDescriptor& tileId) const
    {
        QReadLocker locker(&m_memoryCacheLock);
        return m_memoryCache.contains(tileId.key());
    }

    bool ImageManager::isTileImagePresent(const QString& sourceId,
                                          int x, int y, int zoom) const
    {
        return isTileImagePresent(TileDescriptor{sourceId, x, y, zoom});
    }

    void ImageManager::insertTileToMemoryCache(const QByteArray& tileKey, const QPixmap& pixmap)
    {
        QWriteLocker locker(&m_memoryCacheLock);

        if (!pixmap.isNull())
        {
            DBG_SLEEP(5);
            int cost = pixmap.width() * pixmap.height() * pixmap.depth() / 8;
            m_memoryCache.insert(tileKey, new QPixmapCacheEntry(pixmap), cost);
        }

        QMAP_DBG() << "ImageManager: pixmap cache -> total size MiB: "
                   << QString::number(m_memoryCache.totalCost() / (1024.0 * 1024.0), 'f', 2)
                   << " / "
                   << QString::number(m_memoryCache.maxCost() / (1024.0 * 1024.0), 'f', 1)
                   << " total items: " << m_memoryCache.count()
                   << ", now inserted: " << tileKey;
    }

    bool ImageManager::findTileInMemoryCache(const QByteArray& tileKey, QPixmap& pixmap) const
    {
        // when we read from cache it reordes inside
        // to be able to track the LRU behaviour
        // therefore it needs a writelocker even for the read
        //
        // https://bugreports.qt.io/browse/QTBUG-19794
        QWriteLocker locker(&m_memoryCacheLock);

        const QPixmap *entry = m_memoryCache.object(tileKey);
        if (entry != nullptr)
        {
            pixmap = *entry;
            return true;
        }

        return false;
    }
}

