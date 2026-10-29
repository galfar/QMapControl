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
#include <QObject>
#include <QMutex>
#include <QUrl>
#include <QPixmap>
#include <QTimer>
#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkProxy>

// Local includes.
#include "qmapcontrol_global.h"
#include "INetworkAccessManager.hpp"

/*!
 * @author Kai Winter <kaiwinter@gmx.de>
 * @author Chris Stylianou <chris5287@gmail.com>
 */

namespace qmapcontrol
{
    // Q_NAMESPACE can appear only once for Q_ENUM_NS to work
    // https://bugreports.qt.io/browse/QTBUG-68611
    Q_NAMESPACE

    enum class RequestPurpose
    {
        /// Tile image is requested by map layer painting for immediate display
        Display,
        /// Tile image is requested by map layer painting for possible display in near future
        Prefetch,
        /// Tile data is to be cached to disk from remote source
        Cache
    };
    Q_ENUM_NS(RequestPurpose)

    enum class Failure
    {
        Cancelled,           ///< Caller requested cancellation
        GivenUp,             ///< Gave up trying to get this
        ContentDoesNotExist, ///< Asking for wrong content (like non-existant zoom level)
        ContentDenied,       ///< Don't try again; server is telling us no-go!
        CantConnect          ///< Issues we can't recover from short-term;
    };
    Q_ENUM_NS(Failure);

    class QMAPCONTROL_EXPORT NetworkManager : public QObject
    {
        Q_OBJECT

    public:
        //! Constructor.
        /*!
         * This construct a Network Manager.
         * @param parent QObject parent ownership.
         */
        explicit NetworkManager(INetworkAccessManager* networkAccessManager,
                                QObject* parent = nullptr);

        //! Disable copy constructor.
        NetworkManager(const NetworkManager&) = delete;

        //! Disable copy assignment.
        NetworkManager& operator=(const NetworkManager&) = delete;

        //! Destructor.
        ~NetworkManager() override;

        /*!
         * Set the network proxy to use.
         * @param proxy The network proxy to use.
         * @param userName Proxy user name if needed
         * @param password Proxy password if needed
         */
        void setProxy(const QNetworkProxy& proxy,
                      const QString& userName = QString(),
                      const QString& password = QString());

        /*!
         * Aborts all current downloading threads.
         * This is useful when changing the zoom-factor, though newly needed images loads faster
         */
        void abortDownloads();

        /*!
        * Get the number of current downloads.
        * @return size of the downloading queues.
        */
        int downloadQueueSize() const;

        /*! return default suggested user agent */
        static QByteArray defaultUserAgent();

    public slots:
        /*!
         * Downloads an image resource for the given url.
         * @param url The tile url to download.
         */
        void downloadTile(const QUrl& url);

    signals:
        /*!
         * Signal emitted when a resource has been queued for download.
         * @param count The current size of the download queue.
         */
        void downloadingInProgress(const int count);

        /*!
         * Signal emitted when a resource download has finished and the current download queue is empty.
         */
        void downloadingFinished();

        /*!
         * Signal emitted when a tile has been downloaded.
         * @param tileData Raw tile data
         */
        void tileDownloaded(const QUrl& url, const QByteArray& tileData);

        /*!
         * Signal emitted when tile download fails for reasons other than cancellation.
         * \param url Url that the image was downloaded from.
         * \param failure Informs about failure
         */
        void tileDownloadFailed(const QUrl& url, Failure failure);

    private slots:
        /*!
         * Slot to ask user for proxy authentication details.
         * @param proxy The proxy details.
         * @param authenticator The authenticator to fill out (username/password).
         */
        void proxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator);

        /*!
         * Slot to handle a download that has finished.
         * @param reply The reply that contains the downloaded data.
         */
        void downloadFinished(QNetworkReply* reply);

        /*! Set maximum transfer time before request is considered timed out
          */
        void setTimeout(int timeout_ms);

    private:
        INetworkAccessManager* m_accessManager;

        /// Downloading image queue.
        QMap<QNetworkReply*, QUrl> m_downloadRequests;
        using RequestsIterator = QMapIterator<QNetworkReply*, QUrl>;
        using MutableRequestsIterator = QMutableMapIterator<QNetworkReply*, QUrl>;

        /// Mutex protecting downloading image queue.
        mutable QMutex m_mutex_downloading_image;

        int m_timeout_ms = 30*1000;
        QString m_proxyUserName;
        QString m_proxyPassword;
        QByteArray m_userAgentHeader;

        void requestDownload(const QUrl& url);
    };
}

