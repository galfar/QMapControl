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

#include "ImageManager.h"

// Qt includes.
#include <QCryptographicHash>
#include <QDateTime>
#include <QPainter>
#include <QImageReader>
#include <QBuffer>

// Local includes.
#include "Projection.h"


namespace qmapcontrol
{
    const int kDefaultTileSizePx = 256;
    const int kDefaultPixmapCacheSizeMiB = 30;

    namespace
    {
        /// Singleton instance of Image Manager.
        std::unique_ptr<ImageManager> m_instance = nullptr;
    }

    ImageManager& ImageManager::get()
    {
        // Does the singleton instance exist?
        if(m_instance == nullptr)
        {
            qDebug() << "Spawning new ImageManager";
            // Create a default instance
            m_instance.reset(new ImageManager(kDefaultTileSizePx));
        }

        Q_ASSERT(m_instance);
        // Return the reference to the instance object.
        return *(m_instance.get());
    }

    void ImageManager::destroy()
    {
        // Ensure the singleton instance is destroyed.
        m_instance.reset(nullptr);
    }

    ImageManager::ImageManager(const int tile_size_px, QObject* parent)
        : QObject(parent),
          m_tile_size_px(tile_size_px),
          m_diskCache(nullptr),
          m_cachePolicy(CachePolicy::AlwaysCache),
          m_tileProvider(nullptr)
    {
        setMemoryCacheCapacity(kDefaultPixmapCacheSizeMiB);
        // Setup a loading/empty pixmaps
        setupPlaceholderPixmaps();

        // Connect signal/slot for image downloads.
        connect(this, &ImageManager::downloadImage, &m_networkManager, &NetworkManager::downloadImage);
        connect(&m_networkManager, &NetworkManager::imageDownloaded, this, &ImageManager::handleImageDownloaded);
        connect(&m_networkManager, &NetworkManager::imageCached, this, &ImageManager::handleImageCached);
        connect(&m_networkManager, &NetworkManager::imageDownloadFailed, this, &ImageManager::imageDownloadFailed);
        connect(&m_networkManager, &NetworkManager::downloadingInProgress, this, &ImageManager::downloadingInProgress);
        connect(&m_networkManager, &NetworkManager::downloadingFinished, this, &ImageManager::downloadingFinished);
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
        // Set the proxy on the network manager.
        m_networkManager.setProxy(proxy);
    }

    bool ImageManager::configureDiskCache(const QDir& dir, int capacityMiB)
    {
        // Ensure that the path exists (still returns true when path already exists.
        bool success = dir.mkpath(dir.absolutePath());

        // If the path does exist, enable disk cache.
        if (success)
        {
            if (m_diskCache == nullptr) {
                m_diskCache = new QNetworkDiskCache(this);
            }
            m_diskCache->setCacheDirectory(dir.absolutePath());
            m_diskCache->setMaximumCacheSize(static_cast<qint64>(capacityMiB) * 1024 * 1024);
        }
        else
        {
            qWarning() << "Unable to create directory for persistent cache '" << dir.absolutePath() << "'";
        }

        // Return success.
        return success;
    }

    void ImageManager::clearDiskCache() {
        if (m_diskCache != nullptr) {
            m_diskCache->clear();
        }
    }

    void ImageManager::abortLoading()
    {
        // Abort any remaining network manager downloads.
        m_networkManager.abortDownloads();

        m_prefetchUrls.clear();
    }

    int ImageManager::downloadQueueSize() const
    {
        // Return the network manager downloading queue size.
        return m_networkManager.downloadQueueSize();
    }

    QPixmap ImageManager::getImage(const QUrl& url)
    {
        QPixmap pixmap;
        if (findTileInMemoryCache(url, pixmap))
        {
            Q_ASSERT(!pixmap.isNull());
            // Image found in memory cache, use it
            return pixmap;
        }
        return getImageInternal(url);
    }

    QByteArray ImageManager::rawImageFromDiskCache(const QUrl& url) const {
        {
            QMutexLocker locked(&m_tileProviderLock);
            if (m_tileProvider)
            {
                QByteArray data;
                (void)m_tileProvider->getTileData(url, data);
                return data;
            }
        }

        if ((m_diskCache != nullptr))
        {
            auto data = m_diskCache->data(url);
            if (data != nullptr) {
                QByteArray imgData = data->readAll();
                data->close();
                delete data;
                return imgData;
            }
        }
        return QByteArray();
    }

    QPixmap ImageManager::getImageInternal(const QUrl& url)
    {
        {
            QMutexLocker locked(&m_tileProviderLock);
            if (m_tileProvider) {
                QByteArray data;
                if (m_tileProvider->getTileData(url, data)) {
                    QBuffer buffer(&data);
                    return getImageFromDevice(url, &buffer);
                } else {
                    return m_pixmapEmpty;
                }
            }
        }

        // in offline mode, ask cache directly or return empty tile
        if (m_cachePolicy == CachePolicy::AlwaysCache || m_cachePolicy == CachePolicy::PreferCache)
        {
            if (m_diskCache != nullptr)
            {
                auto data = m_diskCache->data(url);
                if (data != nullptr)
                {
                    QPixmap pixmap = getImageFromDevice(url, data);
                    data->close();
                    delete data;
                    return pixmap;
                }
            }

            // In offline mode just look in the caches, no downloads
            if (m_cachePolicy == CachePolicy::AlwaysCache) {
                return m_pixmapEmpty;
            }
        }

        // Emit that we need to download the image using the network manager.
        // Network manager will prefer network over local cache.
        emit downloadImage(url, false);

        // Image not found, return "loading" image
        return m_pixmapLoading;
    }

    QPixmap ImageManager::getImageFromDevice(const QUrl& url, QIODevice* device)
    {
        QImageReader imageReader(device);
        QPixmap pixmap = QPixmap::fromImageReader(&imageReader);

        insertTileToMemoryCache(url, pixmap);
        m_prefetchUrls.remove(url);

        return pixmap;
    }

    void ImageManager::prefetchImage(const QUrl& url)
    {
        QPixmap pixmap;

        // Only if image is not already available
        if (!findTileInMemoryCache(url, pixmap)) {
            // Add the url to the prefetch list.
            m_prefetchUrls.insert(url);
            // Request the image
            (void)getImageInternal(url);
        }
    }

    bool ImageManager::cacheImageToDisk(const QUrl& url)
    {
        if (m_cachePolicy == CachePolicy::AlwaysCache || m_cachePolicy == CachePolicy::PreferCache) {
            if (rawImageFromDiskCache(url).size() > 0) {
                handleImageCached(url);
                return true;
            }
            if (m_cachePolicy == CachePolicy::AlwaysCache) {
                return true;
            }
        }
        // Emit that we need to download the image using the network manager.
        emit downloadImage(url, true);
        return false;
    }

    void ImageManager::setCachePolicy(CachePolicy policy)
    {
        m_cachePolicy = policy;

        if (m_cachePolicy == CachePolicy::AlwaysCache)
        {
            abortLoading();
        }

        if (m_cachePolicy == CachePolicy::AlwaysNetwork)
        {
            m_networkManager.setCache(nullptr);
        }
        else
        {
            m_networkManager.setCache(m_diskCache);
        }
    }

    void ImageManager::setLoadingPixmap(const QPixmap &pixmap)
    {
        m_pixmapLoading = pixmap;
    }

    void ImageManager::setEmptyPixmap(const QPixmap &pixmap)
    {
        m_pixmapEmpty = pixmap;
    }

    void ImageManager::handleImageDownloaded(const QUrl& url, const QPixmap& pixmap)
    {
#ifdef QMAP_DEBUG
        qDebug() << "ImageManager::handleImageDownloaded '" << url << "'";
#endif
        // Is this a prefetch request?
        if (m_prefetchUrls.contains(url))
        {
            // Remove the url from the prefetch list.
            m_prefetchUrls.remove(url);
        }
        else
        {
            // Let the world know we have received an updated image.
            emit imageUpdated(url);
        }

        // Add it to the pixmap cache
        insertTileToMemoryCache(url, pixmap);
    }

    void ImageManager::handleImageCached(const QUrl& url)
    {
        Q_UNUSED(url);
#ifdef QMAP_DEBUG
        qDebug() << "ImageManager::handleImageCached '" << url << "'";
#endif
        emit imageCached();
    }

    void ImageManager::setupPlaceholderPixmaps()
    {
        // Create a new pixmap.
        m_pixmapLoading = QPixmap(m_tile_size_px, m_tile_size_px);

        // Make is transparent.
        m_pixmapLoading.fill(Qt::transparent);

        // Add a pattern.
        QPainter painter(&m_pixmapLoading);
        QBrush brush(Qt::lightGray, Qt::Dense5Pattern);
        painter.fillRect(m_pixmapLoading.rect(), brush);

        // Add "LOADING..." text.
        painter.setPen(Qt::black);
        painter.drawText(m_pixmapLoading.rect(), Qt::AlignCenter, "LOADING...");

        m_pixmapEmpty = QPixmap(m_tile_size_px, m_tile_size_px);
        m_pixmapEmpty.fill(Qt::transparent);
    }

    QByteArray ImageManager::hashTileUrl(const QUrl& url) const
    {
        // Return the md5 hex value of the given url at a specific projection and tile size.
        return QCryptographicHash::hash((url.toString()
                                           + QString::number(projection::get().epsg())
                                           + QString::number(m_tile_size_px)).toUtf8(),
                                        QCryptographicHash::Md5).toHex();
    }

    void ImageManager::setMemoryCacheCapacity(int capacityMiB)
    {
        m_memoryCache.setMaxCost(capacityMiB * 1024 * 1024);
    }

    class QPixmapCacheEntry : public QPixmap
    {
    public:
        QPixmapCacheEntry(const QPixmap &pixmap) : QPixmap(pixmap) { }
    };

    void ImageManager::insertTileToMemoryCache(const QUrl& url, const QPixmap& pixmap)
    {
        QWriteLocker locker(&m_tileCacheLock);

        if (!pixmap.isNull()) {
            int cost = pixmap.width() * pixmap.height() * pixmap.depth() / 8;
            m_memoryCache.insert(hashTileUrl(url), new QPixmapCacheEntry(pixmap), cost);
        }

#ifdef QMAP_DEBUG
        qDebug() << "ImageManager: pixmap cache -> total size KiB: " << m_memoryCache.totalCost() / 1024
                 << ", now inserted: " << url.toString();
#endif
    }

    bool ImageManager::findTileInMemoryCache(const QUrl& url, QPixmap& pixmap) const
    {
        QReadLocker locker(&m_tileCacheLock);

        QPixmap *entry = m_memoryCache.object(hashTileUrl(url));
        if (entry != nullptr) {
            pixmap = *entry;

#ifdef QMAP_DEBUG
        qDebug() << "ImageManager: found in pixmap cache: " << url.toString();
#endif

            return true;
        }

        return false;
    }

    void ImageManager::setCustomTileProvider(ITileProvider *provider) {
        // weakness: does not stop pending redrawing,
        // therefore custom provider might still receive requests made by current redrawing
        // with urls for different source (there is no "abortRedrawing")
        qDebug() << "ImageManager: request set provider " << provider;
        QMutexLocker tileProviderLock(&m_tileProviderLock);
        abortLoading();
        m_tileProvider = provider;
    }
}



