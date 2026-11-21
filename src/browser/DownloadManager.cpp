#include "browser/DownloadManager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>
#include <QWebEngineDownloadRequest>
#include <QWebEngineProfile>

namespace sora {

void DownloadManager::attach(QWebEngineProfile* profile)
{
    connect(profile, &QWebEngineProfile::downloadRequested, this, [this](QWebEngineDownloadRequest* request) {
        if (request->isSavePageDownload() && askSavePath) {
            const QString path = askSavePath(QDir(request->downloadDirectory()).filePath(request->downloadFileName()));
            if (path.isEmpty()) {
                request->cancel();
                return;
            }
            request->setDownloadDirectory(QFileInfo(path).absolutePath());
            request->setDownloadFileName(QFileInfo(path).fileName());
        }
        connect(request, &QWebEngineDownloadRequest::stateChanged, this, &DownloadManager::changed);
        connect(request, &QWebEngineDownloadRequest::isPausedChanged, this, &DownloadManager::changed);
        request->accept();
        m_items.append(request);
        emit started(request->downloadFileName());
        emit changed();
    });
}

QWebEngineDownloadRequest* DownloadManager::item(int id) const
{
    return id >= 0 && id < m_items.size() ? m_items.at(id).data() : nullptr;
}

QVariantList DownloadManager::list() const
{
    using R = QWebEngineDownloadRequest;
    QVariantList out;
    for (int id = int(m_items.size()) - 1; id >= 0; --id) {
        const R* d = m_items.at(id);
        if (!d)
            continue; // borrada con clearFinished o con su perfil
        QString state;
        switch (d->state()) {
        case R::DownloadRequested:
        case R::DownloadInProgress: state = d->isPaused() ? QStringLiteral("paused") : QStringLiteral("progress"); break;
        case R::DownloadCompleted: state = QStringLiteral("done"); break;
        case R::DownloadCancelled: state = QStringLiteral("cancelled"); break;
        case R::DownloadInterrupted: state = QStringLiteral("failed"); break;
        }
        out.append(QVariantMap{{QStringLiteral("id"), id},
                               {QStringLiteral("name"), d->downloadFileName()},
                               {QStringLiteral("url"), d->url().toString()},
                               {QStringLiteral("path"), QDir(d->downloadDirectory()).filePath(d->downloadFileName())},
                               {QStringLiteral("received"), d->receivedBytes()},
                               {QStringLiteral("total"), d->totalBytes()},
                               {QStringLiteral("state"), state}});
    }
    return out;
}

void DownloadManager::cancel(int id)
{
    if (auto* d = item(id))
        d->cancel();
}

void DownloadManager::pause(int id)
{
    if (auto* d = item(id))
        d->pause();
}

void DownloadManager::resume(int id)
{
    if (auto* d = item(id))
        d->resume();
}

void DownloadManager::open(int id) const
{
    if (auto* d = item(id); d && d->state() == QWebEngineDownloadRequest::DownloadCompleted)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(d->downloadDirectory()).filePath(d->downloadFileName())));
}

void DownloadManager::showInFolder(int id) const
{
    auto* d = item(id);
    if (!d)
        return;
    const QString path = QDir::toNativeSeparators(QDir(d->downloadDirectory()).filePath(d->downloadFileName()));
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + path});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(d->downloadDirectory()));
#endif
}

void DownloadManager::clearFinished()
{
    for (auto& d : m_items)
        if (d && d->isFinished())
            d = nullptr; // el id de las demás no cambia
    emit changed();
}

} // namespace sora
