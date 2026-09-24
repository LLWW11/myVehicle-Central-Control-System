#include "MusicEngine.h"

#include "MusicCache.h"
#include "network/Downloader.h"
#include "network/MusicApi.h"
#include "player/AlsaPlayer.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QUrl>

// 连接一次会话所需的网络、下载和播放器事件
MusicEngine::MusicEngine(QObject *parent)
    : QObject(parent),
      m_player(new AlsaPlayer(this)),
      m_api(new MusicApi(this)),
      m_downloader(new Downloader(this)),
      m_songs(presetSongs())
{
    m_player->setVolume(m_volumePercent);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &MusicEngine::shutdown);

    connect(m_api, &MusicApi::urlReady, this,
            [this](quint64 session, const QString &songId, const QUrl &url)
            {
                if (!isCurrent(session, songId))
                    return;
                const QString destination = MusicCache::mp3File(
                    m_current.source, songId, m_current.quality);
                if (destination.isEmpty())
                {
                    setError(ResourceError, QStringLiteral("无法创建 MP3 缓存路径"));
                    setStatus(InvalidMedia);
                    return;
                }
                m_downloader->download(url, destination, session);
            });
    connect(m_api, &MusicApi::urlError, this,
            [this](quint64 session, const QString &songId,
                   const QString &message)
            {
                if (!isCurrent(session, songId))
                    return;
                setError(NetworkError, message);
                setStatus(InvalidMedia);
            });
    connect(m_api, &MusicApi::lyricReady, this,
            [this](quint64 session, const QString &songId,
                   const QString &text)
            {
                if (!isCurrent(session, songId))
                    return;
                MusicCache::saveLrc(m_current.source, songId, text);
                emit lyricReady(text);
            });
    connect(m_api, &MusicApi::coverReady, this,
            [this](quint64 session, const QString &songId,
                   const QByteArray &data)
            {
                if (!isCurrent(session, songId))
                    return;
                QImage image;
                if (!image.loadFromData(data))
                    return;
                if (image.width() > 800 || image.height() > 800)
                    image = image.scaled(800, 800, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
                QByteArray jpeg;
                QBuffer buffer(&jpeg);
                if (!buffer.open(QIODevice::WriteOnly) ||
                    !image.save(&buffer, "JPG", 85))
                    return;
                buffer.close();
                if (!MusicCache::saveCover(m_current.source, songId, jpeg))
                    return;
                m_coverPath = MusicCache::coverUrl(m_current.source, songId);
                emit coverPathChanged();
            });
    connect(m_downloader, &Downloader::finished, this,
            [this](quint64 session, const QString &path)
            {
                if (session == m_sessionId && m_foregroundAllowed && !m_shutdown)
                    prepareLocalFile(path, session);
            });
    connect(m_downloader, &Downloader::failed, this,
            [this](quint64 session, const QString &message)
            {
                if (session != m_sessionId || m_shutdown)
                    return;
                setError(NetworkError, message);
                setStatus(InvalidMedia);
            });
    connect(m_player, &AlsaPlayer::stateChanged, this,
            [this](quint64 session, PlayerState state)
            {
                if (session != m_sessionId || m_shutdown)
                    return;
                if (state == PlayerState::Playing)
                    setPlaybackState(PlayingState);
                else if (state == PlayerState::Paused)
                    setPlaybackState(PausedState);
                else if (state == PlayerState::Stopped)
                    setPlaybackState(StoppedState);
            });
    connect(m_player, &AlsaPlayer::positionChanged, this,
            [this](quint64 session, qint64 ms)
            {
                if (session != m_sessionId || m_shutdown)
                    return;
                m_position = ms;
                emit positionChanged(ms);
            });
    connect(m_player, &AlsaPlayer::durationChanged, this,
            [this](quint64 session, qint64 ms)
            {
                if (session != m_sessionId || m_shutdown)
                    return;
                m_duration = ms;
                setSeekable(ms > 0 && m_hasAudio);
                emit durationChanged(ms);
            });
    connect(m_player, &AlsaPlayer::errorOccurred, this,
            [this](quint64 session, PlaybackError category,
                   const QString &message)
            {
                if (session != m_sessionId || m_shutdown)
                    return;
                // 只清理本应用的损坏下载缓存，绝不删除用户选取的本地文件。
                if ((category == PlaybackError::FileOpenFailed ||
                     category == PlaybackError::DecodeFailed) &&
                    !m_current.songmid.isEmpty() &&
                    m_localFile == MusicCache::mp3File(
                                       m_current.source, m_current.songmid, m_current.quality))
                {
                    QFile::remove(m_localFile);
                    setHasAudio(false);
                    setSeekable(false);
                }
                setError(category == PlaybackError::Unsupported
                             ? FormatError
                             : ResourceError,
                         message);
                setStatus(InvalidMedia);
            });
    connect(m_player, &AlsaPlayer::playbackEnded, this,
            [this](quint64 session, PlaybackFinishReason reason)
            {
                if (session != m_sessionId || m_shutdown)
                    return;
                if (reason == PlaybackFinishReason::EndOfStream)
                {
                    m_wantsPlay = false;
                    setStatus(EndOfMedia);
                    emit naturalPlaybackEnded();
                }
            });
}

// 最终释放网络和播放任务
MusicEngine::~MusicEngine()
{
    shutdown();
}

// 返回当前播放状态
int MusicEngine::playbackState() const { return m_playbackState; }
// 返回当前播放位置
qint64 MusicEngine::position() const { return m_position; }
// 返回当前歌曲时长
qint64 MusicEngine::duration() const { return m_duration; }
// 返回 QML 使用的归一化音量
qreal MusicEngine::volume() const { return m_volumePercent / 100.0; }
// 把界面音量转换为软件音量百分比
void MusicEngine::setVolume(qreal value)
{
    const int percent = qBound(0, qRound(value * 100), 100);
    if (percent == m_volumePercent)
        return;
    m_volumePercent = percent;
    m_player->setVolume(percent);
    emit volumeChanged(volume());
}
// 返回当前文件的跳转可用状态
bool MusicEngine::seekable() const { return m_seekable; }
// 返回当前会话的本地文件就绪状态
bool MusicEngine::hasAudio() const { return m_hasAudio; }
// 返回当前窗口的前台许可
bool MusicEngine::foregroundAllowed() const { return m_foregroundAllowed; }
// 返回文件准备状态
int MusicEngine::status() const { return m_status; }
// 返回最近错误分类
int MusicEngine::error() const { return m_error; }
// 返回所选歌曲源
QString MusicEngine::source() const { return m_source; }
// 返回当前歌曲标题
QString MusicEngine::songName() const { return m_current.name; }
// 返回当前歌手
QString MusicEngine::singer() const { return m_current.singer; }
// 返回当前专辑
QString MusicEngine::albumName() const { return m_current.albumName; }
// 返回当前本地封面 URL
QString MusicEngine::coverPath() const { return m_coverPath; }

// 查找预置歌曲
int MusicEngine::songIndexForId(const QString &id) const
{
    for (int i = 0; i < m_songs.size(); ++i)
        if (m_songs.at(i).songmid == id)
            return i;
    return -1;
}

// 拒绝旧会话和旧歌曲的异步结果
bool MusicEngine::isCurrent(quint64 sessionId, const QString &songId) const
{
    return !m_shutdown && m_foregroundAllowed &&
           sessionId == m_sessionId && songId == m_current.songmid;
}

// 静默取消旧音乐地址、歌词、封面和 MP3 下载
void MusicEngine::cancelNetwork()
{
    m_api->cancelAll();
    m_downloader->abort();
}

// 选歌只负责准备资源，播放由 play() 明确请求
void MusicEngine::setSource(const QString &source)
{
    if (m_shutdown || source.isEmpty() || source == m_source)
        return;
    ++m_sessionId;
    cancelNetwork();
    m_player->stop(PlaybackFinishReason::SourceChanged);
    m_wantsPlay = false;
    m_localFile.clear();
    m_boundPlayerSession = 0;
    setHasAudio(false);
    setSeekable(false);
    setPlaybackState(StoppedState);
    m_position = 0;
    m_duration = 0;
    emit positionChanged(0);
    emit durationChanged(0);
    setError(NoError, QString());
    m_coverPath.clear();
    emit coverPathChanged();
    emit lyricReady(QString());

    m_source = source;
    emit sourceChanged(source);
    const QUrl url(source);
    QString localPath;
    if (url.isLocalFile())
        localPath = url.toLocalFile();
    else if (QFileInfo(source).isFile())
        localPath = QFileInfo(source).absoluteFilePath();
    if (!localPath.isEmpty())
    {
        m_current = Song{};
        m_current.name = QFileInfo(localPath).completeBaseName();
        m_current.source = QStringLiteral("local");
        emit songChanged();
        prepareLocalFile(localPath, m_sessionId);
        return;
    }
    const int index = songIndexForId(source);
    if (index < 0)
    {
        m_current = Song{};
        emit songChanged();
        setError(ResourceError, QStringLiteral("未找到歌曲或本地文件"));
        setStatus(InvalidMedia);
        return;
    }
    m_current = m_songs.at(index);
    emit songChanged();
    if (m_foregroundAllowed)
    {
        loadCurrentSong();
        requestMetadata();
    }
    else
    {
        setStatus(NoMedia);
    }
}

// 有缓存直接准备本地路径，否则请求播放直链
void MusicEngine::loadCurrentSong()
{
    if (m_current.songmid.isEmpty() || !m_foregroundAllowed)
        return;
    if (MusicCache::hasMp3(m_current.source, m_current.songmid,
                           m_current.quality))
    {
        prepareLocalFile(MusicCache::mp3File(
                             m_current.source, m_current.songmid, m_current.quality),
                         m_sessionId);
        return;
    }
    setStatus(LoadingMedia);
    m_api->requestUrl(m_current.source, m_current.songmid,
                      m_current.quality, m_sessionId);
}

// 注册当前本地 MP3，按用户播放意图决定是否启动
void MusicEngine::prepareLocalFile(const QString &path, quint64 sessionId)
{
    if (sessionId != m_sessionId || !QFileInfo(path).isFile())
        return;
    m_localFile = path;
    m_player->setLocalFile(path, sessionId);
    m_boundPlayerSession = sessionId;
    setHasAudio(true);
    // 解码器报告真实时长后才允许用户 seek。
    setSeekable(false);
    setStatus(LoadedMedia);
    if (m_wantsPlay && m_foregroundAllowed)
        m_player->play();
}

// 从缓存读取或异步请求歌词和封面
void MusicEngine::requestMetadata()
{
    const QString lyric = MusicCache::loadLrc(
        m_current.source, m_current.songmid);
    if (!lyric.isEmpty())
        emit lyricReady(lyric);
    else
        m_api->requestLyric(m_current.source, m_current.songmid, m_sessionId);

    const QString cached = MusicCache::coverUrl(
        m_current.source, m_current.songmid);
    if (!cached.isEmpty())
    {
        QFile file(QUrl(cached).toLocalFile());
        if (file.open(QIODevice::ReadOnly) &&
            file.read(3) == QByteArray::fromHex("ffd8ff"))
        {
            m_coverPath = cached;
            emit coverPathChanged();
            return;
        }
    }
    m_api->downloadCover(QUrl(m_current.img),
                         m_current.songmid, m_sessionId);
}

// 记录播放请求，异步准备完成后会自动启动
void MusicEngine::play()
{
    if (m_shutdown || !m_foregroundAllowed || m_source.isEmpty())
        return;
    m_wantsPlay = true;
    if (m_hasAudio)
    {
        if (m_boundPlayerSession != m_sessionId)
        {
            m_player->setLocalFile(m_localFile, m_sessionId);
            m_boundPlayerSession = m_sessionId;
        }
        m_player->play();
    }
    else if (m_status != LoadingMedia && !m_current.songmid.isEmpty())
    {
        loadCurrentSong();
    }
}

// 暂停当前任务，不触发新的文件准备
void MusicEngine::pause()
{
    m_wantsPlay = false;
    m_player->pause();
}

// 手动停止、取消请求并保留已选歌曲
void MusicEngine::stop()
{
    if (m_shutdown)
        return;
    ++m_sessionId;
    m_wantsPlay = false;
    cancelNetwork();
    m_player->stop(PlaybackFinishReason::UserStopped);
    setPlaybackState(StoppedState);
    setSeekable(false);
    m_position = 0;
    emit positionChanged(0);
    setStatus(m_hasAudio ? LoadedMedia : NoMedia);
}

// 仅对已准备好的本地音频发送 seek
void MusicEngine::seek(qint64 positionMs)
{
    if (m_seekable && m_foregroundAllowed &&
        m_playbackState != StoppedState)
        m_player->seek(qBound<qint64>(0, positionMs, m_duration));
}

// 通过列表索引选择并播放一首预置歌曲
void MusicEngine::playIndex(int index)
{
    if (index < 0 || index >= m_songs.size())
        return;
    setSource(m_songs.at(index).songmid);
    play();
}

// 窗口隐藏时停止与取消；重新显示只恢复用户操作权限
void MusicEngine::setForegroundAllowed(bool allowed)
{
    if (m_shutdown || m_foregroundAllowed == allowed)
        return;
    m_foregroundAllowed = allowed;
    emit foregroundAllowedChanged(allowed);
    if (!allowed)
    {
        ++m_sessionId;
        m_wantsPlay = false;
        cancelNetwork();
        m_player->stop(PlaybackFinishReason::WindowHidden);
        setPlaybackState(StoppedState);
        setSeekable(false);
        m_position = 0;
        emit positionChanged(0);
        // 下载已取消，重新显示后的手动播放必须能够重新发起准备。
        setStatus(m_hasAudio ? LoadedMedia : NoMedia);
    }
}

// 最终关闭时使全部会话失效并同步回收线程
void MusicEngine::shutdown()
{
    if (m_shutdown)
        return;
    m_shutdown = true;
    ++m_sessionId;
    m_wantsPlay = false;
    cancelNetwork();
    m_player->shutdown();
}

// 更新并通知本地音频就绪状态
void MusicEngine::setHasAudio(bool ready)
{
    if (m_hasAudio == ready)
        return;
    m_hasAudio = ready;
    emit hasAudioChanged(ready);
}
// 更新并通知 seek 可用状态
void MusicEngine::setSeekable(bool ready)
{
    if (m_seekable == ready)
        return;
    m_seekable = ready;
    emit seekableChanged(ready);
}
// 更新并通知播放状态
void MusicEngine::setPlaybackState(int state)
{
    if (m_playbackState == state)
        return;
    m_playbackState = state;
    emit playbackStateChanged(state);
}
// 更新并通知媒体准备状态
void MusicEngine::setStatus(int status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged(status);
}
// 更新错误分类并在有错误时发出说明
void MusicEngine::setError(int error, const QString &message)
{
    if (m_error != error)
    {
        m_error = error;
        emit errorChanged(error);
    }
    if (error != NoError && !message.isEmpty())
        emit errorMessage(message);
}
