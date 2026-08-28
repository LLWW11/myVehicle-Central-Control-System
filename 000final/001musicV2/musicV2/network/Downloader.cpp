#include "Downloader.h"

#include <QFile>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

Downloader::Downloader(QObject* parent)
    : QObject(parent)
{
}

void Downloader::download(const QUrl& url, const QString& destPath)
{
    abort();
    m_timedOut = false;

    m_dest = destPath;
    QNetworkRequest req(url);
    QNetworkReply* reply = m_nam.get(req);
    m_reply = reply;

    // 10s 超时(与工程其他网络代码一致); 超时与"主动取消"分开标记,
    // 主动取消(切歌)不当作错误上报
    QTimer* timeout = new QTimer(reply);
    connect(timeout, &QTimer::timeout, reply, [this, reply]() {
        if (m_reply == reply) {   // 只允许当前请求标记超时
            m_timedOut = true;
            reply->abort();
        }
    });
    timeout->start(10000);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_reply == reply)   // 仍是最新请求时才引用
            m_reply = nullptr;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::OperationCanceledError &&
                !m_timedOut) {
                return;   // 被新请求替换而取消, 不算错误
            }
            Q_EMIT failed(m_timedOut ? QStringLiteral("下载超时")
                                   : reply->errorString());
            return;
        }

        QFile f(m_dest);
        if (!f.open(QIODevice::WriteOnly)) {
            Q_EMIT failed(QStringLiteral("无法写入临时文件: ") + m_dest);
            return;
        }
        f.write(reply->readAll());
        f.close();
        Q_EMIT finished(m_dest);
    });
}

void Downloader::abort()
{
    if (m_reply) {
        m_reply->abort();   // finished 信号会被上面的取消分支静默处理
        m_reply = nullptr;
    }
}
