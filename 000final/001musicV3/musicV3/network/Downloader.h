#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QUrl>

// 将一首歌曲分段下载到原子提交的本地文件 
class Downloader final : public QObject
{
    Q_OBJECT
public:
    // 创建 GUI 线程中的异步下载器 
    explicit Downloader(QObject *parent = nullptr);
    // 取消尚未提交的下载并销毁请求上下文 
    ~Downloader() override;
    // 启动新下载，先取消旧任务 
    void download(const QUrl &url, const QString &destination,
                  quint64 sessionId);
    // 取消当前下载，不发失败信号 
    void abort();

signals:
    // 文件完整提交后返回本地路径 
    void finished(quint64 sessionId, const QString &path);
    // 返回当前下载的错误信息 
    void failed(quint64 sessionId, const QString &message);

private:
    struct Request;
    QNetworkAccessManager m_manager;
    QSharedPointer<Request> m_active;
};

#endif // DOWNLOADER_H
