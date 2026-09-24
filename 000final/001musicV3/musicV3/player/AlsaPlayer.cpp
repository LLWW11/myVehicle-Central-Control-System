#include "AlsaPlayer.h"

#include "audio/AudioPlaybackThread.h"

#include <QFileInfo>

// 初始化不带下载器的本地播放器
AlsaPlayer::AlsaPlayer(QObject *parent)
    : QObject(parent)
{
}

// 在应用退出时确保任务资源被回收
AlsaPlayer::~AlsaPlayer()
{
    shutdown();
}

// 记录当前会话的本地文件并异步停止旧任务
void AlsaPlayer::setLocalFile(const QString &path, quint64 sessionId)
{
    if (m_shutdown)
        return;
    if (m_thread && (sessionId != m_sessionId || path != m_currentFile))
        m_thread->requestStop(PlaybackFinishReason::SourceChanged);

    m_currentFile = QFileInfo(path).absoluteFilePath();
    m_sessionId = sessionId;
    m_position = 0;
    m_duration = 0;
    emit positionChanged(sessionId, 0);
    emit durationChanged(sessionId, 0);
}

// 请求启动或恢复当前文件
void AlsaPlayer::play()
{
    if (m_shutdown || m_currentFile.isEmpty())
        return;
    m_playRequested = true;
    if (!m_thread)
    {
        startPlayback();
    }
    else if (m_state == PlayerState::Paused)
    {
        m_thread->requestPause(false);
    }
}

// 暂停现有任务，不改变所选文件
void AlsaPlayer::pause()
{
    if (m_thread && m_state == PlayerState::Playing)
    {
        m_playRequested = false;
        m_thread->requestPause(true);
    }
}

// 停止当前任务，保留本地文件供手动再次播放
void AlsaPlayer::stop(PlaybackFinishReason reason)
{
    m_playRequested = false;
    if (m_thread)
    {
        m_thread->requestStop(reason);
    }
    else
    {
        m_state = PlayerState::Stopped;
        m_position = 0;
        emit stateChanged(m_sessionId, m_state);
        emit positionChanged(m_sessionId, 0);
    }
}

// 把毫秒 seek 命令送入正在运行的任务
void AlsaPlayer::seek(qint64 positionMs)
{
    if (m_thread)
        m_thread->requestSeek(positionMs);
}

// 保存软件音量并通知当前任务
void AlsaPlayer::setVolume(int percent)
{
    m_volume = qBound(0, percent, 100);
    if (m_thread)
        m_thread->setVolume(m_volume);
}

// 仅在最终退出时等待线程结束；重复调用安全
void AlsaPlayer::shutdown()
{
    if (m_shutdown)
        return;
    m_shutdown = true;
    m_playRequested = false;
    if (m_thread)
    {
        m_thread->requestStop(PlaybackFinishReason::ShutDown);
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
}

// 返回最近一次从当前任务收到的状态
PlayerState AlsaPlayer::state() const
{
    return m_state;
}

// 返回最近一次从当前任务收到的位置
qint64 AlsaPlayer::position() const
{
    return m_position;
}

// 返回最近一次从当前任务收到的时长
qint64 AlsaPlayer::duration() const
{
    return m_duration;
}

// 创建一次性工作线程，转发当前会话的结果
void AlsaPlayer::startPlayback()
{
    if (m_thread || !m_playRequested || m_shutdown || m_currentFile.isEmpty())
        return;

    auto *thread = new AudioPlaybackThread(m_currentFile, m_sessionId, this);
    m_thread = thread;
    thread->setVolume(m_volume);
    const quint64 taskSession = m_sessionId;
    m_state = PlayerState::Starting;
    emit stateChanged(taskSession, m_state);

    connect(thread, &AudioPlaybackThread::stateChanged, this,
            [this](quint64 session, PlayerState state)
            {
                if (session != m_sessionId)
                    return;
                m_state = state;
                emit stateChanged(session, state);
            });
    connect(thread, &AudioPlaybackThread::positionChanged, this,
            [this](quint64 session, qint64 positionMs)
            {
                if (session != m_sessionId)
                    return;
                m_position = positionMs;
                emit positionChanged(session, positionMs);
            });
    connect(thread, &AudioPlaybackThread::durationChanged, this,
            [this](quint64 session, qint64 durationMs)
            {
                if (session != m_sessionId)
                    return;
                m_duration = durationMs;
                emit durationChanged(session, durationMs);
            });
    connect(thread, &AudioPlaybackThread::playbackError, this,
            [this](quint64 session, PlaybackError category,
                   const QString &message)
            {
                if (session == m_sessionId)
                    emit errorOccurred(session, category, message);
            });
    connect(thread, &AudioPlaybackThread::playbackEnded, this,
            [this](quint64 session, PlaybackFinishReason reason)
            {
                if (session == m_sessionId)
                {
                    if (reason == PlaybackFinishReason::EndOfStream ||
                        reason == PlaybackFinishReason::Error)
                        m_playRequested = false;
                    emit playbackEnded(session, reason);
                }
            });
    connect(thread, &QThread::finished, this, [this, thread]()
            {
        if (m_thread != thread)
            return;
        m_thread = nullptr;
        thread->deleteLater();
        if (m_playRequested && !m_shutdown)
            startPlayback(); });
    thread->start();
}
