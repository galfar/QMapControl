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

// Local includes.
#include "qmapcontrol_global.h"
#include "NetworkManager.h"
#include "TileDescriptor.hpp"

// Qt includes.
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtGui/QPixmap>
#include <QtNetwork/QNetworkProxy>
#include <QCache>
#include <QReadWriteLock>
#include <QByteArray>

#include <memory>

class QThreadPool;

/*!
 * @author Kai Winter <kaiwinter@gmx.de>
 * @author Chris Stylianou <chris5287@gmail.com>
 */
namespace qmapcontrol
{
    class ITileDiskCache;
    typedef std::function<const QUrl(int x, int y, int zoom)> UrlGenerator;
    typedef std::function<QByteArray(int x, int y, int zoom)> TileDataProvider;
    typedef std::function<QPixmap(int x, int y, int zoom)> TileImageProvider;

    /*!
     * Naming used in ImageManager: tile - encoded tile image data (JPEG, PNG),
     * image - decoded tile image pixmap ready for display.
     */
    class QMAPCONTROL_EXPORT ImageManager : public QObject
    {
        Q_OBJECT
    public:
        enum class CachePolicy {
            // Does not touch tile disk cache at all
            AlwaysNetwork,
            // Requests tile from network, only looks into disk cache when network request fails.
            // Writes retrieved tile to disk cache (can overwrite existing) by default.
            PreferNetwork,
            // First looks in to disk cache and only consults network when not
            // found there. Writes retrieved tile to disk cache (can overwrite existing) by default.
            PreferCache,
            // Does no network requests at all
            AlwaysCache
        };

    public:
        /*!
         * Get the singleton instance of the Image Manager.
         * @return the singleton instance.
         */
        static ImageManager& get();

        static ImageManager& create();

        //! Destroys the singleton instance of Image Manager.
        static void destroy();

    public:
        //! Disable copy constructor.
        ImageManager(const ImageManager&) = delete;

        //! Disable copy assignment.
        ImageManager& operator=(const ImageManager&) = delete;

        //! Destructor.
        ~ImageManager() override;

        /*!
         * Fetch the tile size in pixels.
         * @return the tile size in pixels.
         */
        int tileSizePx() const;

        /*!
         * Set the new tile size (and resets any resources as required).
         * @param tile_size_px The tile size in pixels to set.
         */
        void setTileSizePx(const int tile_size_px);

        /*!
         * Set the network proxy to use.
         * @param proxy The network proxy to use.
         */
        void setProxy(const QNetworkProxy& proxy);

        /*!
         * Aborts all current loading threads.
         * This is useful when changing the zoom-factor.
         */
        void abortLoading();

        /*!
         * Number of images pending in the download queue.
         * @return the number of images pending in the load queue.
         */
        int downloadQueueSize() const;

        /*!
         * Request for a tile image originating at remote location.
         * If the tile is not present locally (in memory cache or optional disk cache)
         * a network query gets started to load it ("loading"
         * placeholder pixmap is returned). Once the image has been downloaded, the image manager
         * will emit "imageUpdated" to inform that the image is now ready.
         *
         * @param sourceId Unique id of tile data source
         * @param x,y,zoom Tile coordinates
         * @param urlGenerator Function that translates tile coordinates to URL for network request
         * @param prefetchOnly True if tile image is to be only placed in memory cache
         * @param maxSourceZoomForUpscale If >0 and <zoom an upscaled part of top tile (@maxZoom) will be returned.
         *   Top tile needs to be present locally otherwise placeholder "empty tile" is returned.
         * @return the pixmap of the tile image or placeholder pixmap.
         */
        QPixmap getImage(const QString& sourceId,
                         int x, int y, int zoom,
                         const UrlGenerator& urlGenerator,
                         bool prefetchOnly, int maxSourceZoomForUpscale);

        /*!
         * Request for a tile image custom local source.
         * Memory cache is first consulted, then if the tile is not fetched from the source "empty tile"
         * placeholder pixmap is returned.
         *
         * @param sourceId Unique id of tile data source
         * @param x, y, zoom Tile coordinates
         * @param tileDataProvider Function that provides raw tile data from external source
         * @param prefetchOnly True if tile image is to be only placed in memory cache
         * @param maxSourceZoomForUpscale If >0 and <zoom an upscaled part of top tile (@maxZoom) will be returned.
         *   Top tile needs to be present otherwise placeholder "empty tile" is returned.
         * @return the pixmap of the tile image or placeholder pixmap.
         */
        QPixmap getImage(const QString& sourceId,
                         int x, int y, int zoom,
                         const TileDataProvider& tileDataProvider,
                         bool prefetchOnly, int maxSourceZoomForUpscale);

        /*!
         * \brief Obtains binary content for a disk-cached url.
         * \param sourceId id of the map source.
         * \param x,y,zoom coordinates of the tile.
         * \return binary content of the url response. Empty if it is not present locally.
         */
        QByteArray tileFromDiskCache(const QString& sourceId,
                                     int x, int y, int zoom) const;

        /*!
         * Downloads tile from network and places it in disk cache (if enabled). Useful
         * for caching some area for later offline use. Cached tiles do not trigger
         * map redraws when received from network nor they are stored in memory cache.
         * \param urlGenerator The Url generator function
         * \param sourceId Descriptor for source
         * \param x,y,zoom Tile coordinate
         * \return true if tile is already in cache, false if network request has spawned and
         *         caller should wait for "tileCached" signal.
         */
        bool cacheImageToDisk(const UrlGenerator& urlGenerator,
                              const QString& sourceId,
                              int x, int y, int zoom);

        /*!
         * \brief clears all tiles stored in the disk cache.
         */
        void clearDiskCache();

        /*!
         * \brief setLoadingPixmap sets the pixmap displayed when a tile is not yet loaded
         * \param pixmap the pixmap to display
         */
        void setLoadingPixmap(const QPixmap &pixmap);

        /*!
         * \brief setEmptyPixmap sets the pixmap displayed when a tile is empty/out of bounds
         * \param pixmap the pixmap to display
         */
        void setEmptyPixmap(const QPixmap &pixmap);

        /*!
         * Enables the persistent disk cache for tiles (also over application restarts),
         * specifying the directory and max size.
         * @param diskCache the ITileDiskCache entity. Ownership is taken by ImageManager
         * @return whether the persistent cache was enabled.
         */
        bool configureDiskCache(ITileDiskCache* diskCache);

        /*!
         * Sets capacity of memory cache for decoded tile images.
         * @param capacityMiB Max cache capacity in MiB, when full LRU images are deleted
         */
        void setMemoryCacheCapacity(int capacityMiB);

        /*!
         * Deletes all tiles in memeory cache.
         */
        void clearMemoryCache();

        /*!
         * Returns true if decoded tile image is present in the memory
         * cache and can be retrieved for display without any delays.
         */
        bool isTileImagePresent(const QString& sourceId,
                                int x, int y, int zoom) const;
        /*!
         * Returns true if decoded tile image is present in the memory
         * cache and can be retrieved for display without any delays.
         */
        bool isTileImagePresent(const TileDescriptor& tileId) const;

        /*!
         * Sets cache policy (default: AlwaysCache or simply "offline")
         * AlwaysNetwork: always pulls tiles from network, cache is not activated.
         * PreferNetwork: tries to pull latest tile from network, othewise goes to cache
         * PreferCache:   pulls tile from cache before asking over network
         * AlwaysCache:   pulls tiles only from cache, there are no network requests
         * \param policy The policy.
         */
        void setDiskCachePolicy(CachePolicy policy);

        /*!
         * Sets the network access manager
         * Needs to be set BEFORE an actual instance of the NetworkManager is created
         */
        void setNetworkManager(INetworkAccessManager* networkAccessManager);

        /*!
         * Return active caching policy.
         */
        CachePolicy diskCachePolicy() const { return m_diskCachePolicy; }

        /*!
         *  Disk cache is not written to regardless of current caching policy.
         */
        void setDiskCacheReadOnly(bool readOnly);

        /// Thread pool used for background operations if Qt's global pool
        /// is not desirable.
        /// \param pool new pool, ImageManager takes ownership
        void setThreadPool(QThreadPool* pool);

        qint64 loadingPixmapCacheKey();
        qint64 emptyPixmapCacheKey();

    signals:
        /*!
         * (PRIVATE) Signal emitted to schedule an tile resource to be downloaded.
         * @param url The tile url to download.
         */
        void downloadTile(const QUrl& url);

        /*!
         * Signal emitted when a new tile has been queued for download.
         * @param count The current size of the download queue.
         */
        void downloadingInProgress(int count);

        /*!
         * Signal emitted when a tile download has finished and the download queue is empty.
         */
        void downloadingFinished();

        /*!
         * Signal emitted when some tile has been downloaded by the network manager,
         * decoded and ready for display.
         */
        void imageUpdated();

        /*!
         * Emited when some tile (requested by cacheImageToDisk()) has been cached to disk.
         */
        void tileCached(TileDescriptor coord,
                        bool servedFromCache,
                        const QByteArray& tileData);

        void tileDownloadSucceeded(TileDescriptor coord, RequestPurpose purpose);

        /*!
          * Emitted when tile download fails for reasons other than cancellation.
          */
        void tileDownloadFailed(TileDescriptor coord, Failure failure, RequestPurpose purpose);

    private slots:
        /*!
         * Slot to handle a tile that has been downloaded.
         * @param url The url that the tile was downloaded from.
         * @param tileData Raw tile data received from network
         */
        void handleTileDownloaded(const QUrl& url,
                                  const QByteArray& tileData);

        /*!
         * Slot to handle a tile download failure.
         * @param url The url that the tile was downloaded from.
         * @param failure Informs about why request failed.
         */
        void handleTileDownloadFailed(const QUrl& url, Failure failure);

    private:

        struct TileRequest;

        //! Constructor.
        /*!
         * This construct a Image Manager.
         * @param tile_size_px The tile size in pixels.
         * @param parent QObject parent ownership.
         */
        ImageManager(const int tile_size_px, QObject* parent = nullptr);

        /*! Create a loading pixmap for use */
        void setupPlaceholderPixmaps();

        /*! Decodes tile data to get tile image pixmap and also puts it in memory cache */
        QPixmap getImageFromByteArray(const QByteArray& tileKey, const QByteArray& data);

        /*! Requests a tile image from memory cache */
        bool findTileInMemoryCache(const QByteArray& tileKey, QPixmap& pixmap) const;

        /*! Puts a new tile image to memory cache */
        void insertTileToMemoryCache(const QByteArray& tileKey, const QPixmap& pixmap);

        /*!
         * Upscales top tile to provide image for a tile in a more detailed
         * zoom level (area covered by the top tile) for use when tile source provides limited
         * zoom levels.
         * \param tile Tile id which needs the upscaled image
         * \param maxSourceZoom Max zoom level that tile source provides data for
         * \param topTileProvider Function that provides tile data for top tile
         * \return Upscaled image or placeholder "empty tile" pixmap
         */
        QPixmap upscaleTopTile(const TileDescriptor& tile, int maxSourceZoom,
                               const TileImageProvider& topTileProvider);

        /// Enables internally network manager
        void enableNetworkManager(bool enable);

        /// Decodes requested tile image, places it into a memory cache, and if needed announces imageUpdated event.
        void decodeRequestedTileImage(const QByteArray& tileData, const TileDescriptor& coord, RequestPurpose purpose);

        QByteArray tileFromDiskCache(const TileDescriptor& tileCoord) const;

    private:

        /// The tile size in pixels.
        int m_tile_size_px;

        ///  Network Access Manager (used by NetworkManager)
        INetworkAccessManager* m_networkAccessManager;

        /// Network manager
        NetworkManager* m_networkManager = nullptr;

        /// Memory cache for decoded tile images
        QCache<QByteArray, QPixmap> m_memoryCache;

        /// Lock for accessing memory tile cache
        mutable QReadWriteLock m_memoryCacheLock;

        /// Local disk cache for tile image files
        ITileDiskCache* m_diskCache;

        /// Active cache policy
        CachePolicy m_diskCachePolicy;

        /// Disk cache is not written to
        bool m_diskCacheReadOnly;

        /// Placeholder pixmap for tile being downloaded.
        QPixmap m_pixmapLoading;

        /// Placeholder pixmap for empty tiles (e.g. out of bounds of offline map)
        QPixmap m_pixmapEmpty;

        /// Collection of active requests
        QMap<QUrl, TileRequest> m_requestedTiles;

        /// Lock for accessing the requests collection
        mutable QReadWriteLock m_requestsLock;

        /// space quota for disk cache
        int m_capacityMiB = 0;

        /// Proxy to be used (only when set)
        QNetworkProxy m_proxy;

        /// Thread pool used for background operations,
        /// by default it's set to QThreadPool::globalInstance()
        QThreadPool* m_threadPool;
    };
}
