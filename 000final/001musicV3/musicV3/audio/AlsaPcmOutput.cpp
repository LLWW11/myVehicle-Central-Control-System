#include "AlsaPcmOutput.h"

AlsaPcmOutput::AlsaPcmOutput()
{
}

AlsaPcmOutput::~AlsaPcmOutput()
{
    close();
}

void AlsaPcmOutput::clearError()
{
    m_errorCode = 0;
    m_errorString.clear();
}

void AlsaPcmOutput::setError(int code, const QString &message)
{
    m_errorCode = code;
    m_errorString = message;
}

void AlsaPcmOutput::setAlsaError(const QString &operation, int alsaError)
{
    m_errorCode = alsaError;
    m_errorString = operation + QStringLiteral("：") +
                    QString::fromLocal8Bit(snd_strerror(alsaError));
}

void AlsaPcmOutput::close()
{
    if (m_pcm)
    {
        snd_pcm_drop(m_pcm);
        snd_pcm_close(m_pcm);
        m_pcm = nullptr;
    }
    m_format = AudioFormat{};
    m_periodFrames = 0;
    m_bufferFrames = 0;
}

bool AlsaPcmOutput::open(const QString &dev,
                         const AudioFormat &format)
{
    close();
    clearError();
    if (!format.isValid())
    {
        setError(-1, QStringLiteral("AudioFormat 参数无效"));
        return false;
    }
    QString actualDeviceName = dev;
    if (actualDeviceName.isEmpty())
    {
        const QByteArray envDevice = qgetenv("CERU_ALSA_DEV");
        actualDeviceName =
            envDevice.isEmpty() ? QStringLiteral("default")
                                : QString::fromLocal8Bit(envDevice);
    }
    const QByteArray deviceUtf8 = actualDeviceName.toUtf8();
    const int openCode = snd_pcm_open(&m_pcm, deviceUtf8.constData(),
                                      SND_PCM_STREAM_PLAYBACK,
                                      SND_PCM_NONBLOCK);
    if (openCode < 0)
    {
        setError(openCode, QStringLiteral("无法打开 ALSA 设备: %1")
                               .arg(snd_strerror(openCode)));
        m_pcm = nullptr;
        return false;
    }
    // 如果不是交错 S16_LE PCM
    if (!configureHardware(format) || !configureSoftware())
    {
        close();
        return false;
    }
    const int prepareCode = snd_pcm_prepare(m_pcm);
    if (prepareCode < 0)
    {
        setAlsaError(QStringLiteral("准备 ALSA 播放设备失败"), prepareCode);
        close();
        return false;
    }

    m_format = format;
    return true;
}

bool AlsaPcmOutput::configureHardware(const AudioFormat &format)
{
    snd_pcm_hw_params_t *params = nullptr;
    snd_pcm_hw_params_alloca(&params);

    int code = snd_pcm_hw_params_any(m_pcm, params);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("获取 ALSA 硬件参数失败"), code);
        return false;
    }
    // 交错
    code = snd_pcm_hw_params_set_access(m_pcm, params,
                                        SND_PCM_ACCESS_RW_INTERLEAVED);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("ALSA 不支持交错 PCM"), code);
        return false;
    }
    // 有符号16位，小端
    code = snd_pcm_hw_params_set_format(m_pcm, params,
                                        SND_PCM_FORMAT_S16_LE);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("ALSA 不支持 S16_LE PCM"), code);
        return false;
    }
    // 通道数
    code = snd_pcm_hw_params_set_channels(m_pcm, params,
                                          static_cast<unsigned int>(format.channels));
    if (code < 0)
    {
        setAlsaError(QStringLiteral("ALSA 不支持当前声道数"), code);
        return false;
    }
    // 采样率
    unsigned int actualRate = static_cast<unsigned int>(format.sampleRate);
    int direction = 0;
    // _near是为了在期望和实际支持的采样率之间选一个最近的
    code = snd_pcm_hw_params_set_rate_near(m_pcm, params,
                                           &actualRate, &direction);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("设置 ALSA 采样率失败"), code);
        return false;
    }
    if (actualRate != static_cast<unsigned int>(format.sampleRate))
    {
        setError(-1, QStringLiteral("ALSA 无法使用解码器的采样率：%1 Hz")
                         .arg(format.sampleRate));
        return false;
    }
    // buffer 占 4个周期大小，每个周期1024帧
    snd_pcm_uframes_t requestedPeriod = 1024;
    code = snd_pcm_hw_params_set_period_size_near(m_pcm, params,
                                                  &requestedPeriod, &direction);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("设置 ALSA period 大小失败"), code);
        return false;
    }
    snd_pcm_uframes_t requestedBuffer = requestedPeriod * 4;
    code = snd_pcm_hw_params_set_buffer_size_near(m_pcm, params,
                                                  &requestedBuffer);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("设置 ALSA 缓冲区大小失败"), code);
        return false;
    }

    code = snd_pcm_hw_params(m_pcm, params);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("应用 ALSA 硬件参数失败"), code);
        return false;
    }

    code = snd_pcm_hw_params_get_period_size(params, &m_periodFrames,
                                             &direction);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("读取 ALSA period 大小失败"), code);
        return false;
    }

    code = snd_pcm_hw_params_get_buffer_size(params, &m_bufferFrames);
    if (code < 0 || m_periodFrames == 0 ||
        m_bufferFrames < m_periodFrames)
    {
        setError(-1, QStringLiteral("ALSA 返回的缓冲区参数无效"));
        return false;
    }

    return true;
}

bool AlsaPcmOutput::configureSoftware()
{
    snd_pcm_sw_params_t *params = nullptr;
    snd_pcm_sw_params_alloca(&params);
    int code = snd_pcm_sw_params_current(m_pcm, params);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("读取 ALSA 软件参数失败"), code);
        return false;
    }
    // 一段PCM写入立即播放
    code = snd_pcm_sw_params_set_start_threshold(m_pcm, params,
                                                 m_periodFrames);

    if (code < 0)
    {
        setAlsaError(QStringLiteral("设置 ALSA 启动阈值失败"), code);
        return false;
    }

    // 声卡至少空出一个 period 时唤醒写入方
    code = snd_pcm_sw_params_set_avail_min(m_pcm, params,
                                           m_periodFrames);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("设置 ALSA 可写阈值失败"), code);
        return false;
    }

    // 应用参数
    code = snd_pcm_sw_params(m_pcm, params);
    if (code < 0)
    {
        setAlsaError(QStringLiteral("应用 ALSA 软件参数失败"), code);
        return false;
    }

    return true;
}
// 只写一段交错 S16PCM
AlsaWriteResult AlsaPcmOutput::writeSome(const qint16 *interleavedPcm,
                                         qint64 frameCount)
{
    if (!m_pcm || !interleavedPcm || frameCount <= 0)
    {
        setError(-1, QStringLiteral("ALSA 写入参数无效"));
        return {AlsaWriteStatus::Error, 0};
    }
    //  单次不超过 ALSA 当前缓冲区容量
    const qint64 requestFrames = qMin(frameCount,
                                      static_cast<qint64>(m_bufferFrames));

    const snd_pcm_sframes_t writtenFrames = snd_pcm_writei(m_pcm, interleavedPcm,
                                                           static_cast<snd_pcm_uframes_t>(requestFrames));
    if (writtenFrames > 0)
        return {AlsaWriteStatus::Written, static_cast<qint64>(writtenFrames)};
    // 流量控制，buffer满了返回-EAGAIN
    if (writtenFrames == 0 || writtenFrames == -EAGAIN)
        return {AlsaWriteStatus::WouldBlock, 0};

    // 可恢复的错误
    if (!recover(static_cast<int>(writtenFrames)))
        return {AlsaWriteStatus::Error, 0};

    // 恢复可能已丢弃设备队列，通知线程从听到的位置重新定位。
    return {AlsaWriteStatus::Recovered, 0};
}

AlsaWaitResult AlsaPcmOutput::waitWritable(int timeoutMs)
{
    if (!m_pcm)
    {
        setError(-1, QStringLiteral("ALSA 设备未打开"));
        return AlsaWaitResult::Error;
    }
    const int waitCode = snd_pcm_wait(m_pcm, qMax(0, timeoutMs));

    if (waitCode > 0)
        return AlsaWaitResult::Ready;
    if (waitCode == 0 || waitCode == -EAGAIN || waitCode == -EINTR)
        return AlsaWaitResult::TimedOut;
    if (!recover(waitCode))
        return AlsaWaitResult::Error;
    return AlsaWaitResult::Recovered;
}

bool AlsaPcmOutput::recover(int alsaError)
{
    // 挂起时只尝试一次恢复；仍不可用就 prepare，避免无限重试。
    int recoverCode = 0;
    if (alsaError == -ESTRPIPE) {
        recoverCode = snd_pcm_resume(m_pcm);
        if (recoverCode == -EAGAIN)
            recoverCode = snd_pcm_prepare(m_pcm);
    } else {
        recoverCode = snd_pcm_recover(m_pcm, alsaError, 1);
    }
    if (recoverCode < 0)
    {
        setAlsaError(QStringLiteral("恢复 ALSA 错误失败"), recoverCode);
        return false;
    }
    return true;
}

bool AlsaPcmOutput::dropAndPrepare()
{
    if (!m_pcm)
        return true;
    const int dropCode = snd_pcm_drop(m_pcm);
    if (dropCode < 0)
    {
        setAlsaError(QStringLiteral("丢弃 ALSA 缓冲区失败"), dropCode);
        return false;
    }

    int prepareCode = snd_pcm_prepare(m_pcm);
    if (prepareCode < 0)
    {
        setAlsaError(QStringLiteral("重新准备 ALSA 设备失败"), prepareCode);
        return false;
    }

    return true;
}

AlsaDrainResult AlsaPcmOutput::drainStep()
{
    if (!m_pcm)
        return AlsaDrainResult::Finished;

    const int drainCode = snd_pcm_drain(m_pcm);

    if (drainCode == 0)
        return AlsaDrainResult::Finished;

    if (drainCode == -EAGAIN)
        return AlsaDrainResult::InProgress;

    setAlsaError(QStringLiteral("排空 ALSA 缓冲区失败"), drainCode);
    return AlsaDrainResult::Error;
}
qint64 AlsaPcmOutput::delayFrames() const
{
    if (!m_pcm)
        return -1;

    snd_pcm_sframes_t delay = 0;
    const int delayCode = snd_pcm_delay(m_pcm, &delay);

    if (delayCode < 0)
        return -1;

    return delay > 0 ? static_cast<qint64>(delay) : 0;
}

/** @brief 返回 ALSA 实际协商的 period 帧数。 */
qint64 AlsaPcmOutput::periodFrames() const
{
    return static_cast<qint64>(m_periodFrames);
}

/** @brief 返回 ALSA 实际协商的缓冲区帧数。 */
qint64 AlsaPcmOutput::bufferFrames() const
{
    return static_cast<qint64>(m_bufferFrames);
}

/** @brief 返回最近一次 ALSA 错误码。 */
int AlsaPcmOutput::errorCode() const
{
    return m_errorCode;
}

/** @brief 返回最近一次 ALSA 错误说明。 */
const QString &AlsaPcmOutput::errorString() const
{
    return m_errorString;
}
