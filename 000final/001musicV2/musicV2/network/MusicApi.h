#ifndef MUSICAPI_H
#define MUSICAPI_H

#include <QByteArray>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

// 音源 URL 请求封装(从 000CMu14 原工程移植, 含歌词/封面)
//   GET https://source.shiqianjiang.cn/music/url?source={s}&songId={id}&quality={q}
//   Headers: X-API-Key / User-Agent / Content-Type
// 返回 { code: 200, url: "..." }
class MusicApi : public QObject {
    Q_OBJECT
public:
    explicit MusicApi(QObject* parent = nullptr);

    // 异步请求播放 URL；结果通过信号返回
    void requestUrl(const QString& source, const QString& songId,
                    const QString& quality = QStringLiteral("320k"));

    // 异步请求歌词(仅 wy 音源)；结果通过信号返回
    void requestLyric(const QString& source, const QString& songId);

    // 异步下载封面图片(原始字节)；结果通过信号返回
    void downloadCover(const QString& url, const QString& songId);

Q_SIGNALS:
    void urlReady(const QString& url);   // 成功，返回 CDN 直链
    void urlError(const QString& msg);   // 失败(403/429/500/超时/解析错误)
    void lyricReady(const QString& lrc); // 歌词成功(原始 LRC 文本)
    void lyricError(const QString& msg); // 歌词失败
    void coverReady(const QString& songId, const QByteArray& data); // 封面成功(图片原始字节)
    void coverError(const QString& msg);     // 封面失败

private Q_SLOTS:
    void onReply();
    void onTimeout();
    void onLyricReply();

private:
    QNetworkAccessManager* m_nam;
    QNetworkReply* m_currentReply = nullptr;
    QTimer* m_timeout;
    bool m_timedOut = false;   // 区分"超时取消"与"主动取消(abortCurrent)"
    void abortCurrent();   // 取消当前请求(新请求前/超时时调用)
};

#endif // MUSICAPI_H
