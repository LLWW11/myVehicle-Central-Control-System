#include "MusicEngine.h"

#include "MusicCache.h"
#include "network/MusicApi.h"
#include "player/AlsaPlayer.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QBuffer>
#include <QImage>

MusicEngine::MusicEngine(QObject* parent)
    : QObject(parent)
    , m_player(new AlsaPlayer(this))
    , m_api(new MusicApi(this))
    , m_songs(presetSongs())
{
    connect(m_api, &MusicApi::urlReady, this, &MusicEngine::onUrlReady);
    connect(m_api, &MusicApi::urlError, this, &MusicEngine::onUrlError);
    connect(m_api, &MusicApi::lyricReady, this, &MusicEngine::onLyricReadyResult);
    connect(m_api, &MusicApi::coverReady, this, &MusicEngine::onCoverReady);

    connect(m_player, &AlsaPlayer::positionChanged,
            this, &MusicEngine::onPlayerPosition);
    connect(m_player, &AlsaPlayer::durationChanged,
            this, &MusicEngine::onPlayerDuration);
    connect(m_player, &AlsaPlayer::errorOccurred,
            this, &MusicEngine::onPlayerError);
    connect(m_player, &AlsaPlayer::stateChanged,
            this, &MusicEngine::onPlayerStateChanged);

    m_player->setVolume(m_vol100);
    m_player->setDownloadDir(MusicCache::dir());
}

int MusicEngine::playbackState() const
{
    return m_playbackState;
}

qint64 MusicEngine::position() const
{
    return m_player->position();
}

qint64 MusicEngine::duration() const
{
    return m_player->duration();
}

qreal MusicEngine::volume() const
{
    return m_vol100 / 100.0;
}

void MusicEngine::setVolume(qreal v)
{
    const int val = qBound(0, int(qRound(v * 100)), 100);
    if (val == m_vol100)
        return;
    m_vol100 = val;
    m_player->setVolume(val);
    Q_EMIT volumeChanged(volume());
}

bool MusicEngine::hasAudio() const
{
    return m_hasFile;
}

bool MusicEngine::autoPlay() const
{
    return m_autoPlay;
}

void MusicEngine::setAutoPlay(bool b)
{
    if (m_autoPlay == b)
        return;
    m_autoPlay = b;
    Q_EMIT autoPlayChanged(b);
}

int MusicEngine::status() const
{
    return m_status;
}

int MusicEngine::error() const
{
    return m_error;
}

QString MusicEngine::source() const
{
    return m_source;
}

QString MusicEngine::songName() const
{
    return m_current.name;
}

QString MusicEngine::singer() const
{
    return m_current.singer;
}

QString MusicEngine::albumName() const
{
    return m_current.albumName;
}

QString MusicEngine::coverPath() const
{
    return m_coverPath;
}

int MusicEngine::songIndexForId(const QString& id) const
{
    for (int i = 0; i < m_songs.size(); ++i) {
        if (m_songs.at(i).songmid == id)
            return i;
    }
    return -1;
}

void MusicEngine::setSource(const QString& src)
{
    if (src.isEmpty() || src == m_source)
        return;

    m_source = src;
    Q_EMIT sourceChanged(src);

    const int idx = songIndexForId(src);
    if (idx < 0)
        return;

    m_current = m_songs.at(idx);
    m_hasFile = false;
    m_switching = true;
    m_pendingPlay = false;

    m_player->stop();

    const QUrl u(src);
    if (u.isLocalFile() || QFileInfo::exists(src)) {
        m_hasFile = true;
        setStatus(LoadedMedia);
        m_player->setUrl(src);
    } else {
        loadCurrentSong();
    }

    Q_EMIT songChanged();
    requestLyricAndCover();
}

void MusicEngine::loadCurrentSong()
{
    setStatus(LoadingMedia);
    if (MusicCache::hasMp3(m_current.songmid)) {
        m_hasFile = true;
        m_player->setUrl(MusicCache::mp3File(m_current.songmid));
        return;
    }
    m_hasFile = false;
    m_api->requestUrl(m_current.source, m_current.songmid, m_current.quality);
}

void MusicEngine::requestLyricAndCover()
{
    const QString lrc = MusicCache::loadLrc(m_current.songmid);
    if (!lrc.isEmpty()) {
        Q_EMIT lyricReady(lrc);
    } else {
        m_api->requestLyric(m_current.source, m_current.songmid);
    }

    const QString cached = MusicCache::coverUrl(m_current.songmid);
    const QString localFile = cached.startsWith(QLatin1String("file://"))
                              ? QUrl(cached).toLocalFile() : QString();

    // 只认真正的 JPEG 缓存(FF D8 FF 开头)。
    // 网易 CDN 部分封面是 PNG 内容(.jpg 后缀), 嵌入式 Qt 可能解不了,
    // 旧缓存里也存过这类坏数据, 一律删掉重新下载再转码。
    bool cacheValid = false;
    if (!localFile.isEmpty()) {
        QFile f(localFile);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray head = f.read(3);
            cacheValid = head.size() == 3
                         && quint8(head[0]) == 0xFF
                         && quint8(head[1]) == 0xD8
                         && quint8(head[2]) == 0xFF;
        }
    }

    if (cacheValid) {
        if (m_coverPath != cached) {
            m_coverPath = cached;
            Q_EMIT coverPathChanged();
        }
        return;
    }

    // 无缓存或缓存损坏: 清掉旧封面避免显示上一首歌的图, 然后重新下载
    if (!localFile.isEmpty())
        QFile::remove(localFile);
    if (!m_coverPath.isEmpty()) {
        m_coverPath.clear();
        Q_EMIT coverPathChanged();
    }
    m_api->downloadCover(m_current.img, m_current.songmid);
}

void MusicEngine::onUrlReady(const QString& url)
{
    m_hasFile = false;
    m_player->playUrl(url, MusicCache::mp3File(m_current.songmid));
}

void MusicEngine::onUrlError(const QString& msg)
{
    m_switching = false;
    m_error = NetworkError;
    Q_EMIT errorChanged(m_error);
    setStatus(InvalidMedia);
    Q_EMIT errorMessage(QStringLiteral("获取播放地址失败: ") + msg);
}

void MusicEngine::onLyricReadyResult(const QString& lrc)
{
    MusicCache::saveLrc(m_current.songmid, lrc);
    Q_EMIT lyricReady(lrc);
}

void MusicEngine::onCoverReady(const QString& songId, const QByteArray& data)
{
    // 校验下载内容确实是可解码的图片(防止 HTML 错误页/截断数据污染缓存)
    QImage img;
    if (!img.loadFromData(data)) {
        qWarning() << "[MusicEngine] 封面数据无效, 已丢弃:" << songId;
        return;
    }

    // 统一转成真正的 JPEG: CDN 部分封面是 PNG 内容(.jpg 后缀),
    // 嵌入式 Qt 可能缺少 PNG 解码; 同时缩到 800px 内, 加快解码和模糊
    if (img.width() > 800 || img.height() > 800)
        img = img.scaled(800, 800, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QByteArray jpeg;
    QBuffer buf(&jpeg);
    if (!buf.open(QIODevice::WriteOnly))
        return;
    if (!img.save(&buf, "JPG", 85)) {
        qWarning() << "[MusicEngine] 封面转码失败:" << songId;
        return;
    }
    buf.close();

    MusicCache::saveCover(songId, jpeg);

    // 下载完成时用户已切到别的歌: 只落盘, 不更新界面
    if (m_current.songmid != songId)
        return;

    const QString file = MusicCache::coverUrl(songId);
    if (!file.isEmpty() && m_coverPath != file) {
        m_coverPath = file;
        Q_EMIT coverPathChanged();
    }
}

void MusicEngine::onPlayerStateChanged(PlayerState state)
{
    switch (state) {
    case PlayerState::Playing:
        m_switching = false;
        m_pendingPlay = false;
        setPlaybackState(PlayingState);
        if (m_status != LoadedMedia)
            setStatus(LoadedMedia);
        Q_EMIT hasAudioChanged(true);
        break;
    case PlayerState::Paused:
        setPlaybackState(PausedState);
        break;
    case PlayerState::Stopped:
        if (!m_switching) {
            setPlaybackState(StoppedState);
            setStatus(EndOfMedia);
            Q_EMIT hasAudioChanged(false);
        }
        break;
    }
}

void MusicEngine::onPlayerPosition(qint64 ms)
{
    Q_EMIT positionChanged(ms);
}

void MusicEngine::onPlayerDuration(qint64 ms)
{
    Q_EMIT durationChanged(ms);
}

void MusicEngine::onPlayerError(const QString& msg)
{
    m_switching = false;
    m_error = ResourceError;
    Q_EMIT errorChanged(m_error);
    setStatus(InvalidMedia);
    Q_EMIT errorMessage(msg);
}

void MusicEngine::play()
{
    switch (m_playbackState) {
    case PausedState:
        m_player->play();
        return;
    case PlayingState:
        return;
    default:
        break;
    }

    if (m_hasFile) {
        m_player->play();
        return;
    }
    if (MusicCache::hasMp3(m_current.songmid)) {
        loadCurrentSong();
        return;
    }
    m_pendingPlay = true;
    if (m_source.isEmpty() || m_status == LoadingMedia)
        return;
    loadCurrentSong();
}

void MusicEngine::pause()
{
    if (m_playbackState == PlayingState)
        m_player->pause();
}

void MusicEngine::stop()
{
    m_switching = true;
    m_player->stop();
    setStatus(NoMedia);
    m_hasFile = false;
}

void MusicEngine::seek(qint64 ms)
{
    m_player->seek(ms);
}

void MusicEngine::playIndex(int index)
{
    if (index < 0 || index >= m_songs.size())
        return;
    m_current = m_songs.at(index);
    m_source = m_current.songmid;
    Q_EMIT sourceChanged(m_source);
    setSource(m_source);
    play();
}

void MusicEngine::setPlaybackState(int st)
{
    if (m_playbackState == st)
        return;
    m_playbackState = st;
    Q_EMIT playbackStateChanged(st);
}

void MusicEngine::setStatus(int st)
{
    if (m_status == st)
        return;
    m_status = st;
    Q_EMIT statusChanged(st);
}