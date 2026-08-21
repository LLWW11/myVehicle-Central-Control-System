#include "MusicApi.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

// 音源 API 配置(与 000CMu14 原工程一致)
static const char* kApiUrl    = "https://source.shiqianjiang.cn/music/url";
static const char* kApiKey    = "CERU_KEY-8F992E9A-4011-4403-8E28-CF85A09F038D";
static const char* kUserAgent = "CeruMusic-Plugin/v7";
static const int   kTimeoutMs = 15000;

// 歌词 API（网易云公开接口）
static const char* kLyricApiUrl = "https://music.163.com/api/song/lyric";
static const int   kLyricTimeoutMs = 10000;

MusicApi::MusicApi(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, &MusicApi::onTimeout);
}

void MusicApi::abortCurrent()
{
    if (m_currentReply) {
        m_currentReply->abort();     // 触发 finished，但 abort 后 error()==OperationCanceledError
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    m_timeout->stop();
}

void MusicApi::requestUrl(const QString& source, const QString& songId,
                          const QString& quality)
{
    // 取消未完成的旧请求，避免回调错乱(用户快速双击场景)
    abortCurrent();
    m_timedOut = false;   // 新请求开始，重置超时标志

    // 构建 URL
    QUrl url(kApiUrl);
    QUrlQuery q;
    q.addQueryItem("source", source);
    q.addQueryItem("songId", songId);
    q.addQueryItem("quality", quality);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-API-Key", kApiKey);
    req.setRawHeader("User-Agent", kUserAgent);

    m_currentReply = m_nam->get(req);
    connect(m_currentReply, &QNetworkReply::finished, this, &MusicApi::onReply);
    m_timeout->start(kTimeoutMs);

    qDebug() << "[MusicApi] request:" << source << songId << quality;
}

void MusicApi::onTimeout()
{
    m_timedOut = true;   // 标记为超时取消(区别于 abortCurrent 主动取消)
    if (m_currentReply) {
        qDebug() << "[MusicApi] timeout, aborting";
        m_currentReply->abort();   // 会触发 onReply，error()==OperationCanceledError
    }
}

void MusicApi::onReply()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // 超时/取消: error()==OperationCanceledError
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        if (m_currentReply == reply) {
            m_currentReply = nullptr;
        }
        reply->deleteLater();
        // 超时取消才报错；abortCurrent 主动取消(换歌)静默
        if (m_timedOut) {
            emit urlError(QStringLiteral("请求超时，请检查网络后重试"));
        }
        return;
    }

    // 网络错误
    if (reply->error() != QNetworkReply::NoError) {
        QString msg = reply->errorString();
        if (m_currentReply == reply) m_currentReply = nullptr;
        reply->deleteLater();
        m_timeout->stop();
        emit urlError(QStringLiteral("网络异常: ") + msg);
        return;
    }

    // 解析 JSON
    QByteArray data = reply->readAll();
    if (m_currentReply == reply) m_currentReply = nullptr;
    reply->deleteLater();
    m_timeout->stop();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        emit urlError(QStringLiteral("响应解析失败: ") + parseErr.errorString());
        return;
    }

    QJsonObject obj = doc.object();
    int code = obj.value("code").toInt();
    QString url = obj.value("url").toString();

    if (code == 200 && !url.isEmpty()) {
        qDebug() << "[MusicApi] url ready:" << url;
        emit urlReady(url);
        return;
    }

    // 按 code 映射友好错误
    QString msg;
    switch (code) {
    case 403:
        msg = QStringLiteral("音源 API Key 失效，请联系维护者");
        break;
    case 429:
        msg = QStringLiteral("请求过于频繁，请稍后再试");
        break;
    case 500:
        msg = QStringLiteral("音源服务器错误: ")
              + obj.value("message").toString(QStringLiteral("未返回 URL"));
        break;
    default:
        msg = QStringLiteral("获取播放链接失败 (code=%1): %2")
              .arg(code).arg(obj.value("message").toString());
        break;
    }
    qDebug() << "[MusicApi] error:" << code << msg;
    emit urlError(msg);
}

void MusicApi::requestLyric(const QString& source, const QString& songId)
{
    // 目前只支持 wy（网易云）歌词接口
    if (source != "wy") {
        emit lyricError(QStringLiteral("当前音源不支持歌词"));
        return;
    }

    // 构建 URL：https://music.163.com/api/song/lyric?os=pc&id={songId}&lv=-1&kv=-1&tv=-1
    QUrl url(kLyricApiUrl);
    QUrlQuery q;
    q.addQueryItem("os", "pc");
    q.addQueryItem("id", songId);
    q.addQueryItem("lv", "-1");
    q.addQueryItem("kv", "-1");
    q.addQueryItem("tv", "-1");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://music.163.com/");

    auto* reply = m_nam->get(req);   // 独立 reply, 不占用 URL 请求的 m_currentReply
    connect(reply, &QNetworkReply::finished, this, &MusicApi::onLyricReply);

    // 超时: reply 销毁时 timer 随之销毁
    QTimer* timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start(kLyricTimeoutMs);

    qDebug() << "[MusicApi] request lyric:" << source << songId;
}

void MusicApi::onLyricReply()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        reply->deleteLater();
        emit lyricError(QStringLiteral("歌词请求超时"));
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        QString msg = reply->errorString();
        reply->deleteLater();
        emit lyricError(QStringLiteral("歌词请求失败: ") + msg);
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        emit lyricError(QStringLiteral("歌词解析失败"));
        return;
    }

    QJsonObject obj = doc.object();

    // 优先使用逐字歌词（yrc），其次 LRC，再其次翻译歌词 —— 与 000CMu14 行为一致
    QString lrcText;
    if (obj.contains("yrc") && obj["yrc"].isObject()) {
        lrcText = obj["yrc"].toObject().value("lyric").toString();
    }
    if (lrcText.isEmpty() && obj.contains("lrc") && obj["lrc"].isObject()) {
        lrcText = obj["lrc"].toObject().value("lyric").toString();
    }
    // 翻译歌词合并到原歌词
    if (obj.contains("tlyric") && obj["tlyric"].isObject()) {
        QString trans = obj["tlyric"].toObject().value("lyric").toString();
        if (!trans.isEmpty() && !lrcText.isEmpty()) {
            lrcText = lrcText + "\n" + trans;
        } else if (!trans.isEmpty()) {
            lrcText = trans;
        }
    }

    if (lrcText.isEmpty()) {
        emit lyricError(QStringLiteral("该歌曲暂无歌词"));
        return;
    }

    qDebug() << "[MusicApi] lyric ready, length:" << lrcText.length();
    emit lyricReady(lrcText);
}

void MusicApi::downloadCover(const QString& url, const QString& songId)
{
    QUrl coverUrl(url);
    QNetworkRequest req(coverUrl);
    req.setRawHeader("User-Agent", "Mozilla/5.0");

    auto* reply = m_nam->get(req);   // 独立 reply
    connect(reply, &QNetworkReply::finished, this, [this, reply, songId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit coverError(reply->errorString());
            return;
        }
        QByteArray data = reply->readAll();
        if (data.isEmpty())
            emit coverError(QStringLiteral("空的封面数据"));
        else
            emit coverReady(songId, data);
    });
}
