#ifndef DOWNLOADER_H
#define DOWNLOADER_H

/*
 * Downloader - 简单文件下载(GUI 线程内异步, 无阻塞)。
 * ALSA 后端播放网络 URL 前, 先下载到本地临时文件再交给 C 引擎。
 */
#include <QNetworkAccessManager>
#include <QObject>

class QNetworkReply;
class QTimer;

class Downloader : public QObject {
    Q_OBJECT
public:
    explicit Downloader(QObject* parent = nullptr);

    void download(const QUrl& url, const QString& destPath);
    void abort();

signals:
    void finished(const QString& path);
    void failed(const QString& msg);

private:
    QNetworkAccessManager m_nam;
    QNetworkReply* m_reply = nullptr;
    bool m_timedOut = false;
    QString m_dest;
};

#endif // DOWNLOADER_H
