#include "Downloader.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>

// 保存单次下载身份、目标文件和取消状态
struct Downloader::Request
{
    // 为本次下载创建未提交的目标文件
    Request(const QString &path, quint64 id)
        : file(path), destination(path), sessionId(id) {}

    QSaveFile file;
    QString destination;
    quint64 sessionId = 0;
    QPointer<QNetworkReply> reply;
    qint64 bytesWritten = 0;
    bool cancelled = false;
    bool timedOut = false;
    QString fileError;
};

// 初始化异步下载器
Downloader::Downloader(QObject *parent) : QObject(parent)
{
}

// 结束下载器时放弃所有未提交的临时数据
Downloader::~Downloader()
{
    abort();
}

// 逐段写入 QSaveFile，完整成功后才提交 MP3
void Downloader::download(const QUrl &url, const QString &destination,
                          quint64 sessionId)
{
    abort();
    if (!url.isValid() ||
        (url.scheme() != QLatin1String("http") &&
         url.scheme() != QLatin1String("https")))
    {
        emit failed(sessionId, QStringLiteral("MP3 下载地址无效"));
        return;
    }
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
    {
        emit failed(sessionId, QStringLiteral("无法创建 MP3 缓存目录"));
        return;
    }
    auto request = QSharedPointer<Request>::create(destination, sessionId);
    if (!request->file.open(QIODevice::WriteOnly))
    {
        emit failed(sessionId, request->file.errorString());
        return;
    }
    m_active = request;
    QNetworkRequest networkRequest(url);
    networkRequest.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply *reply = m_manager.get(networkRequest);
    request->reply = reply;

    // 每个响应保留自己的超时状态，不读取后续下载的成员字段。
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [request, reply]()
            {
        request->timedOut = true;
        reply->abort(); });
    timer->start(30000);

    // 网络到达多少就写多少，避免整首歌曲暂存在内存中。
    const auto appendAvailable = [request, reply]() -> bool
    {
        const QByteArray data = reply->readAll();
        if (data.isEmpty())
            return true;
        const qint64 written = request->file.write(data);
        if (written != data.size())
        {
            request->fileError = request->file.errorString();
            if (request->fileError.isEmpty())
                request->fileError = QStringLiteral("MP3 缓存写入不完整");
            request->file.cancelWriting();
            reply->abort();
            return false;
        }
        request->bytesWritten += written;
        return true;
    };
    connect(reply, &QNetworkReply::readyRead, this, [request, appendAvailable]()
            {
        if (!request->cancelled && request->fileError.isEmpty())
            appendAvailable(); });
    connect(reply, &QNetworkReply::finished, this,
            [this, request, reply, appendAvailable]()
            {
                if (auto *timeout = reply->findChild<QTimer *>())
                    timeout->stop();
                const bool current = (m_active == request);
                if (current)
                    m_active.clear();
                reply->deleteLater();
                if (!current || request->cancelled)
                    return;
                if (!request->fileError.isEmpty())
                {
                    emit failed(request->sessionId, request->fileError);
                    return;
                }
                if (request->timedOut)
                {
                    emit failed(request->sessionId, QStringLiteral("MP3 下载超时"));
                    return;
                }
                if (reply->error() != QNetworkReply::NoError)
                {
                    emit failed(request->sessionId, reply->errorString());
                    return;
                }
                const int status = reply->attribute(
                                            QNetworkRequest::HttpStatusCodeAttribute)
                                       .toInt();
                if (status < 200 || status >= 300)
                {
                    emit failed(request->sessionId,
                                QStringLiteral("MP3 下载 HTTP 状态 %1").arg(status));
                    return;
                }
                if (!appendAvailable() || request->bytesWritten == 0)
                {
                    emit failed(request->sessionId,
                                request->fileError.isEmpty()
                                    ? QStringLiteral("MP3 下载内容为空")
                                    : request->fileError);
                    return;
                }
                if (!request->file.commit())
                {
                    emit failed(request->sessionId, request->file.errorString());
                    return;
                }
                emit finished(request->sessionId, request->destination);
            });
}

// 取消网络请求并放弃尚未提交的临时文件
void Downloader::abort()
{
    if (!m_active)
        return;
    auto request = m_active;
    m_active.clear();
    request->cancelled = true;
    request->file.cancelWriting();
    if (request->reply)
        request->reply->abort();
}
