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

#include "NetworkManager.h"

// Qt includes.
#include <QMutexLocker>
#include <QImageReader>
#include <QAbstractNetworkCache>
#include <QSignalBlocker>

namespace qmapcontrol
{

    QByteArray NetworkManager::defaultUserAgent() {
        // enable organization domain only if sure that accesses to server conform their policies
        return QString("%1/%2 (%3, %4) %5").arg(qApp->applicationName(),
                                                qApp->applicationVersion(),
                                                QSysInfo::kernelType(),
                                                QSysInfo::currentCpuArchitecture(),
                                                ""/*qApp->organizationDomain()*/).toLatin1();
    }

    NetworkManager::NetworkManager(INetworkAccessManager* networkAccessManager,
                                   QObject* parent)
        : QObject(parent),
          m_accessManager(networkAccessManager)
    {
        // Connect signal/slot to handle proxy authentication.
        connect(m_accessManager, &INetworkAccessManager::proxyAuthenticationRequired, this, &NetworkManager::proxyAuthenticationRequired);

        // Connect signal/slot to handle finished downloads.
        connect(m_accessManager, &INetworkAccessManager::finished, this, &NetworkManager::downloadFinished);

        m_userAgentHeader = defaultUserAgent();
        QMAP_DBG() << m_userAgentHeader;
    }

    NetworkManager::~NetworkManager()
    {
        blockSignals(true);
        // Ensure all download queues are aborted.
        abortDownloads();
    }

    void NetworkManager::setProxy(const QNetworkProxy& proxy, const QString& userName, const QString& password)
    {
        m_proxyUserName = userName;
        m_proxyPassword = password;

        // Set the proxy on the network access manager.
        m_accessManager->setProxy(proxy);
    }

    void NetworkManager::abortDownloads()
    {
        QMutexLocker lock(&m_mutex_downloading_image);
        QSignalBlocker blockSignals(m_accessManager); // block cancellations (competition for queue)
        MutableRequestsIterator itr(m_downloadRequests);
        while (itr.hasNext())
        {
            // Tell the reply to abort.
            itr.next().key()->abort();
            // reply->url() may be different from the requested one in case of redirects
            emit tileDownloadFailed(itr.key()->request().url(), Failure::Cancelled);

            itr.key()->deleteLater();
            itr.remove();
        }
        emit downloadingFinished();
        Q_ASSERT(m_downloadRequests.isEmpty());
        m_accessManager->clearConnectionCache();
    }

    void NetworkManager::setTimeout(int timeout_ms) {
        Q_ASSERT(timeout_ms >= 0);
        m_timeout_ms = timeout_ms;
    }

    int NetworkManager::downloadQueueSize() const
    {
        // NOTE: can be called from any thread

        // Default return value.
        int return_size(0);

        // Return the size of the downloading image queue.
        QMutexLocker lock(&m_mutex_downloading_image);
        return_size += m_downloadRequests.size();

        // Return the size.
        return return_size;
    }

    void NetworkManager::downloadTile(const QUrl& url)
    {
        // NOTE: Runs in the main thread

        // Keep track of our success.
        bool success(false);

        // Scope this as we later call "downloadQueueSize()" which also locks all download queue mutexes.
        {
            // Gain a lock to protect the downloading image container.
            QMutexLocker lock(&m_mutex_downloading_image);

            // Check this is a new request.
            RequestsIterator itr(m_downloadRequests);
            if (!itr.findNext(url))
            {
                requestDownload(url);

                // Mark our success.
                success = true;
            }
        }
        // Were we successful?
        if (success)
        {
            // Emit that we are downloading a new image (with details of the current queue size).
            emit downloadingInProgress(downloadQueueSize());
        }
    }

    void NetworkManager::requestDownload(const QUrl& url)
    {
        // NOTE: Runs in the main thread

        // Generate a new request.
        QNetworkRequest request(url);
        request.setRawHeader(QByteArrayLiteral("User-Agent"), m_userAgentHeader);

        // Using pipelining QNAM can put multiple requests in a single packet
        // (just a suggestion and needs server support).
        request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

        // By default QNAM does not follow redirects, we want redirects (but not https->http)
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        // Send the request.
        QNetworkReply* reply = m_accessManager->get(request);

        // setup timeout
        if (m_timeout_ms)
        {
            QTimer::singleShot(m_timeout_ms, reply, [reply]
                                                    {
                                                        QMAP_DBG() << "Aborting!";
                                                        reply->abort();
                                                    } );
        }

        // Store the request into the downloading image queue.
        m_downloadRequests[reply] = url;

        // Log success.
        QMAP_DBG() << "Downloading image '" << url << "', queued: " << m_downloadRequests.size();
    }

    void NetworkManager::proxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator)
    {
        // Log proxy authentication request.
        QMAP_DBG() << "Proxy Authentication Required for '" << proxy.hostName()
                   << "' with the authenticator '" << &authenticator << "'";

        // Set the stored username/password values into the authenticator to use.
        authenticator->setUser(m_proxyUserName);
        authenticator->setPassword(m_proxyPassword);
    }

    void NetworkManager::downloadFinished(QNetworkReply* reply)
    {
        // NOTE: Runs in the main thread
        Q_ASSERT(reply);
        bool hasReply = false;

        // reply->url() may be different from the requested one in case of redirects
        const QUrl requestedUrl = reply->request().url();

        {
            QMutexLocker lock(&m_mutex_downloading_image);
            hasReply = m_downloadRequests.remove(reply) > 0;
            if (!hasReply) {
                qWarning() << "Unexpected reply for: " << requestedUrl;
            }
        }

        if (hasReply)
        {
            QNetworkReply::NetworkError error = reply->error();
            const int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // Did the reply return errors...
            if (error != QNetworkReply::NoError)
            {

                QMAP_DBG() << "Failed to download '" << requestedUrl
                           << "' with error '" << reply->errorString() << "' (" << error << ")"
                           << ", HTTP code: " << httpStatusCode;

                Failure failure;

                switch (error)  {
                    case QNetworkReply::TimeoutError:
                    case QNetworkReply::OperationCanceledError:
                        failure = Failure::GivenUp;
                        break;

                    case QNetworkReply::ContentNotFoundError:
                    case QNetworkReply::ContentGoneError:
                    case QNetworkReply::UnknownContentError:
                    case QNetworkReply::ContentConflictError:
                        failure = Failure::ContentDoesNotExist;
                        break;

                    case QNetworkReply::ContentAccessDenied:
                        // we got successful response it's not sensible to try getting this content
                        failure = Failure::ContentDenied;
                        break;

                    case QNetworkReply::ProxyNotFoundError:
                    case QNetworkReply::ConnectionRefusedError:
                    case QNetworkReply::TooManyRedirectsError:
                    case QNetworkReply::InsecureRedirectError:
                    case QNetworkReply::ContentOperationNotPermittedError:
                    case QNetworkReply::AuthenticationRequiredError:
                    case QNetworkReply::OperationNotImplementedError:
                    case QNetworkReply::ServiceUnavailableError:
                    case QNetworkReply::RemoteHostClosedError:
                        failure = Failure::CantConnect;
                        break;

                    default:
                        QMAP_DBG() << "Unrecoverable: '" << requestedUrl << "'";
                        failure = Failure::CantConnect;
                        break;
                }

                emit tileDownloadFailed(requestedUrl, failure);
            }
            else if ((httpStatusCode < 200) || (httpStatusCode >= 300))
            {
                // QNAM reports QNetworkReply::NoError even in some cases
                // we consider failures (non-followed redirects, ...) -> check HTTP status code as well.
                QMAP_DBG() << "Failed to download '" << requestedUrl << ", bad HTTP status: " << httpStatusCode;
                emit tileDownloadFailed(requestedUrl, Failure::CantConnect);
            }
            else
            {
                QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();

                // Also check for content type. Bad tile data is rejected when
                // decoding for display but it can get into disk caches still.
                if (contentType.startsWith("image/")) {
                    QMAP_DBG() << "Downloaded image " << requestedUrl << ", payload size: " << reply->size()
                               << ", content type: " << contentType;

                    // Emit that we have downloaded a tile
                    emit tileDownloaded(requestedUrl, reply->readAll());
                }
                else
                {
                    QMAP_DBG() << "Failed to download '" << requestedUrl << ", bad content type: " << contentType;
                    emit tileDownloadFailed(requestedUrl, Failure::ContentDoesNotExist);
                }
            }
        }

        // Check if the current download queue is empty.
        if (downloadQueueSize() == 0)
        {
            QMAP_DBG() << "Scheduling finished...";
            // Emit that all queued downloads have finished.
            emit downloadingFinished();

            // since we finished all pending task there is no need for the QNetworkAccessManager cache
            m_accessManager->clearConnectionCache();
        }

        reply->deleteLater();
    }

}
