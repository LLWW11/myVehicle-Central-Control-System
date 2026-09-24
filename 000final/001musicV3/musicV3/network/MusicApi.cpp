#include "MusicApi.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

namespace
{
    const char kPlaybackApi[] = "https://source.shiqianjiang.cn/music/url";
    const char kLyricApi[] = "https://music.163.com/api/song/lyric";
    // 与 musicV2 相同的默认音源密钥；可通过环境变量覆盖，避免依赖板上环境。
    const char kDefaultApiKey[] = "CERU_KEY-8F992E9A-4011-4403-8E28-CF85A09F038D";

    // 密钥优先级：环境变量 > 内置默认值。
    QByteArray resolveApiKey()
    {
        const QByteArray fromEnvironment = qgetenv("CERU_MUSIC_API_KEY");
        return fromEnvironment.isEmpty() ? QByteArray(kDefaultApiKey) : fromEnvironment;
    }
}

// 构造异步音乐 API 客户端
MusicApi::MusicApi(QObject *parent) : QObject(parent)
{
}

// 创建每个响应独立的超时定时器
QNetworkReply *MusicApi::get(const QNetworkRequest &request, int timeoutMs)
{
    QNetworkRequest redirectable(request);
    redirectable.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply *reply = m_manager.get(redirectable);
    m_replies.insert(reply);
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply]()
            {reply->setProperty("musicTimedOut", true);reply->abort(); });
    timer->start(timeoutMs);
    return reply;
}

// 从活跃集合中移除网络响应
void MusicApi::release(QNetworkReply *reply)
{
    m_replies.remove(reply);
    if (auto *timer = reply->findChild<QTimer *>())
        timer->stop();
    reply->deleteLater();
}

// 请求播放地址，API Key 从设备环境变量读取
void MusicApi::requestUrl(const QString &source, const QString &songId,
                          const QString &quality, quint64 sessionId)
{
    const QByteArray key = resolveApiKey();
    if (key.isEmpty())
    {
        emit urlError(sessionId, songId,
                      QStringLiteral("未设置 CERU_MUSIC_API_KEY"));
        return;
    }
    QUrl url(QString::fromLatin1(kPlaybackApi));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("source"), source);
    query.addQueryItem(QStringLiteral("songId"), songId);
    query.addQueryItem(QStringLiteral("quality"), quality);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setRawHeader("X-API-Key", key);
    request.setRawHeader("User-Agent", "CeruMusic-Plugin/v7");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = get(request, 15000);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, sessionId, songId]()
            {
                const bool cancelled = reply->property("musicCancelled").toBool();
                const bool timedOut = reply->property("musicTimedOut").toBool();
                const QNetworkReply::NetworkError error = reply->error();
                const QString networkMessage = reply->errorString();
                const QByteArray body = reply->readAll();
                release(reply);
                if (cancelled)
                    return;
                if (timedOut || error != QNetworkReply::NoError)
                {
                    emit urlError(sessionId, songId,
                                  timedOut ? QStringLiteral("播放地址请求超时") : networkMessage);
                    return;
                }
                const QJsonDocument doc = QJsonDocument::fromJson(body);
                if (!doc.isObject())
                {
                    emit urlError(sessionId, songId, QStringLiteral("播放地址响应格式错误"));
                    return;
                }
                const QJsonObject obj = doc.object();
                const QUrl mediaUrl(obj.value(QStringLiteral("url")).toString());
                if (obj.value(QStringLiteral("code")).toInt() == 200 &&
                    mediaUrl.isValid() &&
                    (mediaUrl.scheme() == QLatin1String("http") ||
                     mediaUrl.scheme() == QLatin1String("https")))
                {
                    emit urlReady(sessionId, songId, mediaUrl);
                }
                else
                {
                    emit urlError(sessionId, songId,
                                  obj.value(QStringLiteral("message")).toString(QStringLiteral("未返回可用的播放地址")));
                }
            });
}

// 请求网易云歌曲的逐字歌词或普通歌词
void MusicApi::requestLyric(const QString &source, const QString &songId,
                            quint64 sessionId)
{
    if (source != QLatin1String("wy"))
        return;
    QUrl url(QString::fromLatin1(kLyricApi));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("os"), QStringLiteral("pc"));
    query.addQueryItem(QStringLiteral("id"), songId);
    query.addQueryItem(QStringLiteral("lv"), QStringLiteral("-1"));
    query.addQueryItem(QStringLiteral("kv"), QStringLiteral("-1"));
    query.addQueryItem(QStringLiteral("tv"), QStringLiteral("-1"));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    request.setRawHeader("Referer", "https://music.163.com/");
    QNetworkReply *reply = get(request, 10000);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, sessionId, songId]()
            {
                const bool cancelled = reply->property("musicCancelled").toBool();
                const bool timedOut = reply->property("musicTimedOut").toBool();
                const QNetworkReply::NetworkError error = reply->error();
                const QString networkMessage = reply->errorString();
                const QByteArray body = reply->readAll();
                release(reply);
                if (cancelled)
                    return;
                if (timedOut || error != QNetworkReply::NoError)
                {
                    emit lyricError(sessionId, songId,
                                    timedOut ? QStringLiteral("歌词请求超时") : networkMessage);
                    return;
                }
                const QJsonDocument doc = QJsonDocument::fromJson(body);
                if (!doc.isObject())
                {
                    emit lyricError(sessionId, songId, QStringLiteral("歌词响应格式错误"));
                    return;
                }
                const QJsonObject obj = doc.object();
                QString lyric = obj.value(QStringLiteral("yrc")).toObject().value(QStringLiteral("lyric")).toString();
                if (lyric.isEmpty())
                    lyric = obj.value(QStringLiteral("lrc")).toObject().value(QStringLiteral("lyric")).toString();
                const QString translated = obj.value(QStringLiteral("tlyric")).toObject().value(QStringLiteral("lyric")).toString();
                if (!translated.isEmpty())
                    lyric += (lyric.isEmpty() ? QString() : QStringLiteral("\n")) + translated;
                if (lyric.isEmpty())
                    emit lyricError(sessionId, songId, QStringLiteral("该歌曲暂无歌词"));
                else
                    emit lyricReady(sessionId, songId, lyric);
            });
}

// 获取封面原始字节，由业务层校验和转换
void MusicApi::downloadCover(const QUrl &url, const QString &songId,
                             quint64 sessionId)
{
    if (!url.isValid() || url.isEmpty())
        return;
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    QNetworkReply *reply = get(request, 10000);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, sessionId, songId]()
            {
                const bool cancelled = reply->property("musicCancelled").toBool();
                const bool timedOut = reply->property("musicTimedOut").toBool();
                const QNetworkReply::NetworkError error = reply->error();
                const QString networkMessage = reply->errorString();
                const QByteArray bytes = reply->readAll();
                release(reply);
                if (cancelled)
                    return;
                if (timedOut || error != QNetworkReply::NoError || bytes.isEmpty())
                {
                    emit coverError(sessionId, songId,
                                    timedOut ? QStringLiteral("封面请求超时") : error != QNetworkReply::NoError ? networkMessage
                                                                                                                : QStringLiteral("封面响应为空"));
                    return;
                }
                emit coverReady(sessionId, songId, bytes);
            });
}

// 主动取消全部旧请求，回调只做资源释放
void MusicApi::cancelAll()
{
    const auto pending = m_replies;
    for (QNetworkReply *reply : pending)
    {
        reply->setProperty("musicCancelled", true);
        reply->abort();
    }
}
