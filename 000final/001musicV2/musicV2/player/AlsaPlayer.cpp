#include "AlsaPlayer.h"

#include "audio/alsa_engine.h"
#include "network/Downloader.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

namespace {

PlayerState toQtState(int s)
{
    switch (s) {
    case ENGINE_PLAYING: return PlayerState::Playing;
    case ENGINE_PAUSED:  return PlayerState::Paused;
    default:             return PlayerState::Stopped;
    }
}

// 以下三个回调运行在 C 引擎的播放线程上,
// 这里只做 emit(自动排队到 GUI 线程), 不调用任何引擎接口
void engineStateCb(int state, void* user)
{
    auto* p = static_cast<AlsaPlayer*>(user);
    emit p->stateChanged(toQtState(state));
}

void engineErrorCb(const char* msg, void* user)
{
    auto* p = static_cast<AlsaPlayer*>(user);
    emit p->errorOccurred(QString::fromUtf8(msg));
}

void engineEndCb(void* user)
{
    Q_UNUSED(user);   // 自然结束: 状态回调已带 IDLE, 无需额外处理
}

} // namespace

AlsaPlayer::AlsaPlayer(QObject* parent)
    : QObject(parent)
    , m_downloader(new Downloader(this))
    , m_timer(new QTimer(this))
{
    engine_init(engineStateCb, engineErrorCb, engineEndCb, this);
    engine_set_volume(80);

    connect(m_downloader, &Downloader::finished,
            this, &AlsaPlayer::onDownloadFinished);
    connect(m_downloader, &Downloader::failed,
            this, &AlsaPlayer::onDownloadFailed);

    // 200ms 轮询进度/时长(从 C 引擎读原子量, 比回调简单可靠)
    m_timer->setInterval(200);
    connect(m_timer, &QTimer::timeout, this, &AlsaPlayer::onPollTimer);
    m_timer->start();
}

AlsaPlayer::~AlsaPlayer()
{
    m_timer->stop();
    engine_stop();     // 等待播放线程退出
    engine_deinit();
}

void AlsaPlayer::setUrl(const QString& url)
{
    if (url.isEmpty())
        return;

    const QUrl u(url);
    if (u.isLocalFile() || QFileInfo::exists(url)) {
        startPlayback(QFileInfo(url).absoluteFilePath());
        return;
    }

    // 网络 URL: 下载到临时目录(按 URL 做缓存文件名), 完成后交给引擎
    const QByteArray hash = QCryptographicHash::hash(url.toUtf8(),
                                                     QCryptographicHash::Md5);
    const QString dir = m_downloadDir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        : m_downloadDir;
    const QString dest = dir + QLatin1String("/ceru_") +
                         QString::fromLatin1(hash.toHex()) +
                         QLatin1String(".mp3");
    qDebug() << "[AlsaPlayer] downloading" << url << "->" << dest;
    m_downloader->download(QUrl(url), dest);
}

void AlsaPlayer::playUrl(const QString& url, const QString& destFile)
{
    if (url.isEmpty())
        return;

    const QUrl u(url);
    if (u.isLocalFile() || QFileInfo::exists(url)) {
        startPlayback(destFile.isEmpty() ? QFileInfo(url).absoluteFilePath()
                                         : destFile);
        return;
    }

    if (!destFile.isEmpty())
        m_downloader->download(QUrl(url), destFile);
    else
        setUrl(url);
}

void AlsaPlayer::setDownloadDir(const QString& dir)
{
    m_downloadDir = dir;
}

void AlsaPlayer::startPlayback(const QString& path)
{
    if (m_hasFile)
        engine_stop();
    m_currentFile = path;
    m_hasFile = true;

    qDebug() << "[AlsaPlayer] play:" << path;
    if (engine_play(path.toUtf8().constData()) != 0) {
        emit errorOccurred(QStringLiteral("启动播放失败: ") +
                           QString::fromUtf8(engine_last_error()));
    }
}

void AlsaPlayer::play()
{
    const int s = engine_get_state();
    if (s == ENGINE_PAUSED) {
        engine_resume();
    } else if (s == ENGINE_IDLE || s == ENGINE_ERROR) {
        if (m_hasFile)
            engine_play(m_currentFile.toUtf8().constData());
    }
    // PLAYING 时重复点播放无动作
}

void AlsaPlayer::pause()  { engine_pause(); }
void AlsaPlayer::stop()   { engine_stop(); }

void AlsaPlayer::setVolume(int v)
{
    engine_set_volume(qBound(0, v, 100));
}

void AlsaPlayer::seek(qint64 ms)
{
    engine_seek(ms);
}

PlayerState AlsaPlayer::state() const
{
    return toQtState(engine_get_state());
}

qint64 AlsaPlayer::position() const { return engine_position_ms(); }
qint64 AlsaPlayer::duration() const { return engine_duration_ms(); }

void AlsaPlayer::onPollTimer()
{
    const qint64 pos = engine_position_ms();
    if (pos != m_lastPos) {
        m_lastPos = pos;
        emit positionChanged(pos);
    }
    const qint64 dur = engine_duration_ms();
    if (dur != m_lastDur) {
        m_lastDur = dur;
        emit durationChanged(dur);
    }
    const PlayerState st = state();
    if (st != m_lastState) {
        m_lastState = st;
        emit stateChanged(st);
    }
}

void AlsaPlayer::onDownloadFinished(const QString& path)
{
    qDebug() << "[AlsaPlayer] download finished:" << path;
    startPlayback(path);
}

void AlsaPlayer::onDownloadFailed(const QString& msg)
{
    emit errorOccurred(QStringLiteral("音频下载失败: ") + msg);
}
