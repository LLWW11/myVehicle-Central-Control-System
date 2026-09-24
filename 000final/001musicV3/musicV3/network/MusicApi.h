#ifndef MUSICAPI_H
#define MUSICAPI_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

class QNetworkReply;
class QNetworkRequest;

/** @brief 按歌曲会话请求播放地址、歌词和封面。 */
class MusicApi final : public QObject
{
    Q_OBJECT
public:
    /** @brief 创建 GUI 线程中的异步网络请求管理器。 */
    explicit MusicApi(QObject *parent = nullptr);
    /** @brief 请求本地 MP3 下载所需的播放直链。 */
    void requestUrl(const QString &source, const QString &songId,
                    const QString &quality, quint64 sessionId);
    /** @brief 请求网易云歌曲歌词。 */
    void requestLyric(const QString &source, const QString &songId,
                      quint64 sessionId);
    /** @brief 下载原始封面字节供上层转为 JPEG。 */
    void downloadCover(const QUrl &url, const QString &songId,
                       quint64 sessionId);
    /** @brief 主动取消全部未完成请求，不发出错误信号。 */
    void cancelAll();

signals:
    /** @brief 当前请求获得音频直链。 */
    void urlReady(quint64 sessionId, const QString &songId, const QUrl &url);
    /** @brief 当前请求未获得音频直链。 */
    void urlError(quint64 sessionId, const QString &songId,
                  const QString &message);
    /** @brief 返回当前歌曲的歌词文本。 */
    void lyricReady(quint64 sessionId, const QString &songId,
                    const QString &text);
    /** @brief 歌词请求失败，不阻断音频。 */
    void lyricError(quint64 sessionId, const QString &songId,
                    const QString &message);
    /** @brief 返回当前歌曲的原始封面字节。 */
    void coverReady(quint64 sessionId, const QString &songId,
                    const QByteArray &data);
    /** @brief 封面请求失败，不阻断音频。 */
    void coverError(quint64 sessionId, const QString &songId,
                    const QString &message);

private:
    /** @brief 发起带独立超时状态的 GET 请求。 */
    QNetworkReply *get(const QNetworkRequest &request, int timeoutMs);
    /** @brief 移除并延迟销毁已完成的网络响应。 */
    void release(QNetworkReply *reply);

    QNetworkAccessManager m_manager;
    QSet<QNetworkReply *> m_replies;
};

#endif // MUSICAPI_H
