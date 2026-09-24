
#ifndef AUDIOPLAYBACKTHREAD_H
#define AUDIOPLAYBACKTHREAD_H

#include "player/PlayerState.h"

#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <QString>
#include <QtGlobal>

// 一次本地 MP3 播放任务；音频资源仅在 run() 中使用
class AudioPlaybackThread final : public QThread
{
    Q_OBJECT
public:
    AudioPlaybackThread(const QString &localPath,
                        quint64 sessionId,
                        QObject *parent = nullptr);

    ~AudioPlaybackThread() override;
    void requestStop(PlaybackFinishReason reason); // 结束本次播放
    void requestPause(bool paused);                // 暂停或回复
    /**  记录最新的毫秒跳转位置 */
    void requestSeek(qint64 positionMs); // 跳转
    void setVolume(int percent);         // 修改音量

signals:
    void stateChanged(quint64 sessionId, PlayerState state);    // 本次任务的播放状态
    void positionChanged(quint64 sessionId, qint64 positionMs); // 当前播放位置
    void durationChanged(quint64 sessionId, qint64 durationMs); // 歌曲时长
    void playbackError(quint64 sessionId, PlaybackError category,
                       const QString &message);                         // 播放错误及文字说明
    void playbackEnded(quint64 sessionId, PlaybackFinishReason reason); // 结束原因

protected:
    void run() override; // 打开、解码和输出一首 MP3，结束前释放所有音频资源

private:
    QString m_localPath;
    quint64 m_sessionId = 0;

    // 以下命令由 GUI 线程写入、音频线程读取，均受互斥锁保护
    QMutex m_mutex;
    QWaitCondition m_wake;

    bool m_stopRequested = false;
    PlaybackFinishReason m_stopReason = PlaybackFinishReason::UserStopped;

    bool m_pauseRequested = false;
    bool m_seekPending = false;
    qint64 m_seekPositionMs = 0;
    int m_volumePercent = 100;
};

#endif /* AUDIOPLAYBACKTHREAD_H */
