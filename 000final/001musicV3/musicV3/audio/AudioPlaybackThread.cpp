#include "AudioPlaybackThread.h"

#include "audio/AlsaPcmOutput.h"
#include "audio/Mpg123Decoder.h"

#include <QElapsedTimer>
#include <QMutexLocker>
#include <QVector>

AudioPlaybackThread::AudioPlaybackThread(const QString &localPath,
                                         quint64 sessionId,
                                         QObject *parent)
    : QThread(parent), m_localPath(localPath), m_sessionId(sessionId)
{
}

// 最终回收线程；普通切歌由播放器门面异步等待 finished 信号
AudioPlaybackThread::~AudioPlaybackThread()
{
    if (isRunning())
    {
        requestStop(PlaybackFinishReason::ShutDown);
        wait();
    }
}

// 登记停止请求并唤醒暂停中的音频线程
void AudioPlaybackThread::requestStop(PlaybackFinishReason reason)
{
    QMutexLocker locker(&m_mutex);
    m_stopRequested = true;
    m_stopReason = reason;
    m_wake.wakeAll();
}

//  登记暂停或恢复请求
void AudioPlaybackThread::requestPause(bool paused)
{
    QMutexLocker locker(&m_mutex);
    m_pauseRequested = paused;
    m_wake.wakeAll();
}

//  仅保留最近一次 seek 请求
void AudioPlaybackThread::requestSeek(qint64 positionMs)
{
    QMutexLocker locker(&m_mutex);
    m_seekPositionMs = qMax<qint64>(0, positionMs);
    m_seekPending = true;
    m_wake.wakeAll();
}

//   音量在下一次解码块处理时生效
void AudioPlaybackThread::setVolume(int percent)
{
    QMutexLocker locker(&m_mutex);
    m_volumePercent = qBound(0, percent, 100);
}

//    执行解码、命令处理、部分写入和自然结束排空
void AudioPlaybackThread::run()
{
    PlaybackFinishReason reason = PlaybackFinishReason::EndOfStream;
    emit stateChanged(m_sessionId, PlayerState::Starting);

    // 所有 mpg123 和 ALSA 调用都在当前音频线程执行
    {
        Mpg123Decoder decoder;
        AlsaPcmOutput output;

        // 错误统一转为带会话编号的信号
        const auto fail = [&](PlaybackError category, const QString &message)
        {
            reason = PlaybackFinishReason::Error;
            emit playbackError(m_sessionId, category, message);
        };
        // 控制锁只用于读取命令，绝不覆盖解码和声卡操作
        const auto stopped = [&]() -> bool
        {
            QMutexLocker locker(&m_mutex);
            if (!m_stopRequested)
                return false;
            reason = m_stopReason;
            return true;
        };

        do
        {
            if (stopped())
                break;
            if (!decoder.open(m_localPath))
            {
                fail(PlaybackError::FileOpenFailed, decoder.errorString());
                break;
            }
            if (stopped())
                break;

            const AudioFormat format = decoder.format();
            const qint64 duration = decoder.durationMs();
            if (duration < 0)
            {
                fail(PlaybackError::DecodeFailed, decoder.errorString());
                break;
            }
            emit durationChanged(m_sessionId, duration);
            if (stopped())
                break;
            if (!output.open(QString(), format))
            {
                fail(PlaybackError::AudioDeviceFailed, output.errorString());
                break;
            }
            if (stopped())
                break;

            constexpr qint64 blockCapacity = 1024;
            QVector<qint16> pcm(static_cast<int>(blockCapacity * format.channels));
            qint64 blockFrames = 0;
            qint64 writtenFromBlock = 0;
            qint64 baseFrames = 0;
            qint64 submittedFrames = 0;
            bool blockEndsFile = false;
            bool paused = false;
            int emptyReads = 0;
            QElapsedTimer progressClock;
            progressClock.start();
            emit stateChanged(m_sessionId, PlayerState::Playing);

            // 当前听到的位置按已提交帧减去 ALSA 排队帧估算
            const auto heardMs = [&]() -> qint64
            {
                const qint64 delay = output.delayFrames();
                const qint64 heard = qMax<qint64>(
                    0, baseFrames + submittedFrames - qMax<qint64>(0, delay));
                return qMin(duration, heard * 1000 / format.sampleRate);
            };

            while (reason == PlaybackFinishReason::EndOfStream)
            {
                bool seek = false;
                bool pause = false;
                qint64 seekMs = 0;
                int volume = 100;
                {
                    QMutexLocker locker(&m_mutex);
                    if (m_stopRequested)
                    {
                        reason = m_stopReason;
                        break;
                    }
                    seek = m_seekPending;
                    seekMs = m_seekPositionMs;
                    m_seekPending = false;
                    pause = m_pauseRequested;
                    volume = m_volumePercent;
                }

                if (seek)
                {
                    if (!output.dropAndPrepare())
                    {
                        fail(PlaybackError::PCMWriteFailed, output.errorString());
                        break;
                    }
                    const qint64 actualMs = decoder.seekMs(qMin(seekMs, duration));
                    if (actualMs < 0)
                    {
                        fail(PlaybackError::DecodeFailed, decoder.errorString());
                        break;
                    }
                    baseFrames = actualMs * format.sampleRate / 1000;
                    submittedFrames = 0;
                    blockFrames = writtenFromBlock = 0;
                    blockEndsFile = false;
                    emit positionChanged(m_sessionId, actualMs);
                }

                if (pause)
                {
                    if (!paused)
                    {
                        // 丢弃声卡队列后回到实际听到的位置，避免暂停后还有尾音
                        const qint64 actualMs = heardMs();
                        if (!output.dropAndPrepare())
                        {
                            fail(PlaybackError::PCMWriteFailed, output.errorString());
                            break;
                        }
                        const qint64 resumedMs = decoder.seekMs(actualMs);
                        if (resumedMs < 0)
                        {
                            fail(PlaybackError::DecodeFailed, decoder.errorString());
                            break;
                        }
                        baseFrames = resumedMs * format.sampleRate / 1000;
                        submittedFrames = 0;
                        blockFrames = writtenFromBlock = 0;
                        blockEndsFile = false;
                        paused = true;
                        emit positionChanged(m_sessionId, resumedMs);
                        emit stateChanged(m_sessionId, PlayerState::Paused);
                    }
                    QMutexLocker locker(&m_mutex);
                    if (!m_stopRequested && m_pauseRequested && !m_seekPending)
                        m_wake.wait(&m_mutex);
                    continue;
                }
                if (paused)
                {
                    paused = false;
                    emit stateChanged(m_sessionId, PlayerState::Playing);
                }

                if (progressClock.elapsed() >= 200)
                {
                    emit positionChanged(m_sessionId, heardMs());
                    progressClock.restart();
                }

                if (writtenFromBlock == blockFrames)
                {
                    if (blockEndsFile)
                        break;
                    const auto decoded = decoder.readFrame(pcm.data(), blockCapacity);
                    if (decoded.hasError)
                    {
                        fail(PlaybackError::DecodeFailed, decoder.errorString());
                        break;
                    }
                    if (decoded.formatChanged || decoder.format() != format)
                    {
                        fail(PlaybackError::Unsupported,
                             QStringLiteral("MP3 播放中途改变了 PCM 格式"));
                        break;
                    }
                    blockFrames = decoded.frameCnt;
                    writtenFromBlock = 0;
                    blockEndsFile = decoded.endOfStream;
                    if (blockFrames == 0)
                    {
                        if (blockEndsFile)
                            break;
                        if (++emptyReads > 4)
                        {
                            fail(PlaybackError::DecodeFailed,
                                 QStringLiteral("解码器连续未产生 PCM"));
                            break;
                        }
                        continue;
                    }
                    emptyReads = 0;
                    // 每块只缩放一次；部分写入重试不重复处理同一批样本
                    for (qint64 i = 0; i < blockFrames * format.channels; ++i)
                    {
                        const int scaled = static_cast<int>(pcm[static_cast<int>(i)]) * volume / 100;
                        pcm[static_cast<int>(i)] =
                            static_cast<qint16>(qBound(-32768, scaled, 32767));
                    }
                }

                const qint16 *next = pcm.constData() + writtenFromBlock * format.channels;
                const qint64 checkpointMs = heardMs();
                const AlsaWriteResult result =
                    output.writeSome(next, blockFrames - writtenFromBlock);
                if (result.status == AlsaWriteStatus::Written)
                {
                    writtenFromBlock += result.frameWritten;
                    submittedFrames += result.frameWritten;
                    continue;
                }
                else if (result.status == AlsaWriteStatus::WouldBlock)
                {
                    const AlsaWaitResult waitResult = output.waitWritable(20);
                    if (waitResult == AlsaWaitResult::Error)
                    {
                        fail(PlaybackError::PCMWriteFailed, output.errorString());
                        break;
                    }
                    if (waitResult != AlsaWaitResult::Recovered)
                        continue;
                }
                else if (result.status != AlsaWriteStatus::Recovered)
                {
                    fail(PlaybackError::PCMWriteFailed, output.errorString());
                    break;
                }
                // 欠载恢复后，已提交帧计数不再等于声卡真实队列
                if (!output.dropAndPrepare())
                {
                    fail(PlaybackError::PCMWriteFailed, output.errorString());
                    break;
                }
                const qint64 actualMs = decoder.seekMs(checkpointMs);
                if (actualMs < 0)
                {
                    fail(PlaybackError::DecodeFailed, decoder.errorString());
                    break;
                }
                else
                {
                    baseFrames = actualMs * format.sampleRate / 1000;
                    submittedFrames = 0;
                    blockFrames = writtenFromBlock = 0;
                    blockEndsFile = false;
                    emit positionChanged(m_sessionId, actualMs);
                }
            }

            if (reason != PlaybackFinishReason::EndOfStream)
                break;

            // 等待尾音期间仍可响应停止与窗口隐藏
            QElapsedTimer drainClock;
            drainClock.start();
            while (!stopped())
            {
                const qint64 delay = output.delayFrames();
                if (delay <= 0)
                    break;
                if (drainClock.elapsed() > 5000)
                {
                    fail(PlaybackError::PCMWriteFailed,
                         QStringLiteral("等待 ALSA 尾音超时"));
                    break;
                }
                QMutexLocker locker(&m_mutex);
                if (!m_stopRequested)
                    m_wake.wait(&m_mutex, 20);
            }
            if (reason != PlaybackFinishReason::EndOfStream)
                break;
            while (!stopped())
            {
                const AlsaDrainResult drained = output.drainStep();
                if (drained == AlsaDrainResult::Finished)
                {
                    emit positionChanged(m_sessionId, duration);
                    break;
                }
                if (drained == AlsaDrainResult::Error || drainClock.elapsed() > 5000)
                {
                    fail(PlaybackError::PCMWriteFailed, output.errorString());
                    break;
                }
                QMutexLocker locker(&m_mutex);
                if (!m_stopRequested)
                    m_wake.wait(&m_mutex, 20);
            }
        } while (false);
        emit stateChanged(m_sessionId, PlayerState::Stopping);
    }

    emit stateChanged(m_sessionId, PlayerState::Stopped);
    emit playbackEnded(m_sessionId, reason);
}
