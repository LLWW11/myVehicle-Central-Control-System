#ifndef ALSAPLAYER_H
#define ALSAPLAYER_H

#include "player/PlayerState.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

class AudioPlaybackThread;

/** @brief GUI 线程中的本地播放器门面，负责一次播放线程的启动与回收。 */
class AlsaPlayer final : public QObject
{
    Q_OBJECT
public:
    /** @brief 创建未加载文件的播放器。 */
    explicit AlsaPlayer(QObject *parent = nullptr);
    /** @brief 最终回收仍在运行的音频线程。 */
    ~AlsaPlayer() override;

    /** @brief 设置已下载的本地 MP3，不自动播放。 */
    void setLocalFile(const QString &path, quint64 sessionId);
    /** @brief 启动或恢复当前本地文件。 */
    void play();
    /** @brief 暂停当前播放。 */
    void pause();
    /** @brief 请求停止；一般路径不阻塞 GUI。 */
    void stop(PlaybackFinishReason reason);
    /** @brief 请求跳转到毫秒位置。 */
    void seek(qint64 positionMs);
    /** @brief 设置软件音量百分比。 */
    void setVolume(int percent);
    /** @brief 最终退出时同步回收线程，可重复调用。 */
    void shutdown();

    /** @brief 返回播放器缓存的播放状态。 */
    PlayerState state() const;
    /** @brief 返回播放器缓存的位置，单位毫秒。 */
    qint64 position() const;
    /** @brief 返回播放器缓存的时长，单位毫秒。 */
    qint64 duration() const;

signals:
    /** @brief 带会话编号的状态变化。 */
    void stateChanged(quint64 sessionId, PlayerState state);
    /** @brief 带会话编号的位置变化。 */
    void positionChanged(quint64 sessionId, qint64 positionMs);
    /** @brief 带会话编号的时长变化。 */
    void durationChanged(quint64 sessionId, qint64 durationMs);
    /** @brief 带会话编号的音频错误。 */
    void errorOccurred(quint64 sessionId, PlaybackError category,
                       const QString &message);
    /** @brief 一次任务结束及结束原因。 */
    void playbackEnded(quint64 sessionId, PlaybackFinishReason reason);

private:
    /** @brief 在没有旧任务时创建并连接新的音频线程。 */
    void startPlayback();

    AudioPlaybackThread *m_thread = nullptr;
    QString m_currentFile;
    quint64 m_sessionId = 0;
    PlayerState m_state = PlayerState::Stopped;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    int m_volume = 80;
    bool m_playRequested = false;
    bool m_shutdown = false;
};

#endif // ALSAPLAYER_H
