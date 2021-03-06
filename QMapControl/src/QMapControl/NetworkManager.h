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
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkProxy>

// Local includes.
#include "qmapcontrol_global.h"

/*!
 * @author Kai Winter <kaiwinter@gmx.de>
 * @author Chris Stylianou <chris5287@gmail.com>
 */
namespace qmapcontrol
{
    class QMAPCONTROL_EXPORT NetworkManager : public QObject
    {
        Q_OBJECT
    public:
        //! Constructor.
        /*!
         * This construct a Network Manager.
         * @param parent QObject parent ownership.
         */
        explicit NetworkManager(QObject* parent = nullptr);

        //! Disable copy constructor.
        NetworkManager(const NetworkManager&) = delete;

        //! Disable copy assignment.
        NetworkManager& operator=(const NetworkManager&) = delete;

        //! Destructor.
        ~NetworkManager();

        /*!
         * Set the network proxy to use.
         * @param proxy The network proxy to use.
         * @param useName Proxy user name if needed
         * @param password Proxy password if needed
         */
        void setProxy(const QNetworkProxy& proxy, const QString& userName = QString(), const QString& password = QString());

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

        /*!
         * Checks if the given url resource is currently being downloaded.
         * @param url The url of the resource.
         * @return boolean, if the url resource is already downloading.
         */
        bool isDownloading(const QUrl& url) const;

        /*!
         * Sets the disk cache for network manager. Useful for filling and updating of local
         * cache for offline usage.
         * @param qCache the disk cache object to set
         */
        void setCache(QAbstractNetworkCache* cache);

    public slots:
        /*!
         * Downloads an image resource for the given url.
         * @param url The image url to download.
         * @param cacheOnly If true image is only stored to disk cache and not for display.
         */
        void downloadImage(const QUrl& url, bool cacheOnly);

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
         * Signal emitted when an image has been downloaded for display.
         * @param url The url that the image was downloaded from.
         * @param pixmap The image.
         */
        void imageDownloaded(const QUrl& url, const QPixmap& pixmap);

        /*!
         * Signal emitted when an image has been downloaded to disk cache.
         * @param url The url that the image was downloaded from.
         * */
        void imageCached(const QUrl& url);

        /*!
         * Signal emitted when image download fails for reasons other than cancellation.
         * \param The url that the image was downloaded from.
         */
        void imageDownloadFailed(const QUrl& url, QNetworkReply::NetworkError error);

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

        /*!
          */
        void abortTimeoutedRequests();

    private:
        QNetworkAccessManager m_accessManager;

        /// Downloading image queue.
        QMap<QNetworkReply*, QUrl> m_downloadRequests;

        /// Mutex protecting downloading image queue.
        mutable QMutex m_mutex_downloading_image;

        QString m_proxyUserName;
        QString m_proxyPassword;

        /// For periodic checks of timeouted requests
        QTimer m_timeoutTimer;

        void requestDownload(const QUrl& url, bool cacheOnly);
    };
}
