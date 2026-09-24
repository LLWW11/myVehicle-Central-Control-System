#include "Mpg123Decoder.h"

#include <cstdio>
#include <limits>
Mpg123Decoder::Mpg123Decoder()
{
}
Mpg123Decoder::~Mpg123Decoder()
{
    close();
}
// 打开文件，并强制 S16 解码
bool Mpg123Decoder::open(const QString &path)
{
    close();
    clearError();
    if (path.isEmpty())
    {
        setError(-1, QStringLiteral("MP3 文件路径为空"));
        return false;
    }
    int createCode = MPG123_OK;
    m_handle = mpg123_new(nullptr, &createCode); // 创建句柄
    if (!m_handle)
    {
        setError(createCode,
                 QString::fromLocal8Bit(mpg123_plain_strerror(createCode)));
        return false;
    }
    const QByteArray utf8Path = path.toUtf8();
    const int openCode = mpg123_open(m_handle, utf8Path.constData()); // 打开mp3
    if (openCode != MPG123_OK)
    {
        setHandleError(openCode);
        close();
        return false;
    }
    // PCM输出格式
    long sampleRate = 0;
    int channels = 0;
    int ignoredEncoding = 0;
    // 读取默认参数
    const int formatCode = mpg123_getformat(m_handle, &sampleRate, &channels, &ignoredEncoding);

    if (formatCode != MPG123_OK)
    {
        setHandleError(formatCode);
        close();
        return false;
    }
    // 清空支持的格式掩码
    const int clearFormatCode = mpg123_format_none(m_handle);

    if (clearFormatCode != MPG123_OK)
    {
        setHandleError(clearFormatCode);
        close();
        return false;
    }
    // 强制：仅允许以 S16 解码输出
    const int setFormatCode = mpg123_format(m_handle, sampleRate,
                                            channels, MPG123_ENC_SIGNED_16);

    if (setFormatCode != MPG123_OK)
    {
        setHandleError(setFormatCode);
        close();
        return false;
    }

    if (!updateFormat())
    {
        close();
        return false;
    }

    return true;
}

// 解码到 pcm 缓冲区
Mpg123Decoder::readResult Mpg123Decoder::readFrame(qint16 *pcm, qint64 capacityFrames)
{
    readResult result;
    if (!isOpen())
    {
        setError(-1, QStringLiteral("MP3打开错误"));
        result.hasError = true;
        return result;
    }
    if (!pcm || capacityFrames <= 0)
    {
        setError(-1, QStringLiteral("PCM 输出缓冲区无效"));
        result.hasError = true;
        return result;
    }
    const int bytesPerFrame = m_format.bytesPerFrame();
    if (bytesPerFrame <= 0)
    {
        setError(-1, QStringLiteral("当前 PCM 格式无效"));
        result.hasError = true;
        return result;
    }

    if (static_cast<quint64>(capacityFrames) >
        std::numeric_limits<size_t>::max() / static_cast<size_t>(bytesPerFrame))
    {
        setError(-1, QStringLiteral("PCM 输出缓冲区过大"));
        result.hasError = true;
        return result;
    }

    const size_t outputBytes = static_cast<size_t>(capacityFrames * bytesPerFrame);

    size_t doneBytes = 0;
    clearError();

    const int readCode = mpg123_read(m_handle, pcm, outputBytes, &doneBytes);
    if (doneBytes % static_cast<size_t>(bytesPerFrame) != 0)
    {
        setError(-1, QStringLiteral("MP3 解码结果不是完整 PCM 帧"));
        result.hasError = true;
        return result;
    }

    result.frameCnt = static_cast<qint64>(doneBytes / static_cast<size_t>(bytesPerFrame));

    if (readCode == MPG123_OK)
    {
        if (result.frameCnt == 0)
        {
            setError(-1, QStringLiteral("MP3 解码未返回 PCM 数据"));
            result.hasError = true;
        }
        return result;
    }
    if (readCode == MPG123_NEW_FORMAT)
    {
        const AudioFormat previousFormat = m_format;
        if (!updateFormat())
        {
            result.hasError = true;
            return result;
        }

        // 首次格式通知可能与原格式一致；真正变化时由播放线程停止。
        result.formatChanged = (m_format != previousFormat);
        return result;
    }
    if (readCode == MPG123_DONE)
    {
        result.endOfStream = true; // 歌曲播放结束
        return result;
    }

    setHandleError(readCode);
    result.hasError = true;
    return result;
}
// 跳转到指定ms位置
qint64 Mpg123Decoder::seekMs(qint64 targetMs)
{
    if (!isOpen() || !m_format.isValid())
    {
        setError(-1, QStringLiteral("MP3 解码器未处于可跳转状态"));
        return -1;
    }
    targetMs = (targetMs < 0) ? 0 : targetMs;
    // 这些所有操作都是基于 S16 来的
    if (targetMs > std::numeric_limits<qint64>::max() / m_format.sampleRate)
    {
        setError(-1, QStringLiteral("目标跳转位置过大"));
        return -1;
    }
    const qint64 targetFrames = targetMs * static_cast<qint64>(m_format.sampleRate) / 1000;
    const qint64 maxOffset = static_cast<qint64>(std::numeric_limits<off_t>::max());
    if (targetFrames > maxOffset)
    {
        setError(-1, QStringLiteral("目标跳转位置超出解码器范围"));
        return -1;
    }

    clearError();

    const off_t actualFrames = mpg123_seek(m_handle,
                                           static_cast<off_t>(targetFrames),
                                           SEEK_SET);

    if (actualFrames < 0)
    {
        setHandleError(static_cast<int>(actualFrames));
        return -1;
    }

    return static_cast<qint64>(actualFrames) * 1000 / m_format.sampleRate;
}

qint64 Mpg123Decoder::durationMs()
{
    if (!isOpen() || !m_format.isValid())
    {
        setError(-1, QStringLiteral("MP3 解码器未处于可扫描状态"));
        return -1;
    }
    if (m_durationMs > 0)
        return m_durationMs;
    clearError();

    const int scanCode = mpg123_scan(m_handle);
    if (scanCode != MPG123_OK)
    {
        setHandleError(scanCode);
        return -1;
    }
    const off_t sampleLength = mpg123_length(m_handle);
    if (sampleLength < 0)
    {
        setError(-1, QStringLiteral("获取 MP3 总样本数出错"));
        return -1;
    }
    if (static_cast<qint64>(sampleLength) > std::numeric_limits<qint64>::max() / 1000)
    {
        setError(-1, QStringLiteral("MP3 时长超出范围"));
        return -1;
    }
    m_durationMs = static_cast<qint64>(sampleLength) * 1000 / m_format.sampleRate;

    if (seekMs(0) < 0)
    {
        m_durationMs = -1;
        return -1;
    }
    return m_durationMs;
}
AudioFormat Mpg123Decoder::format() const
{
    return m_format;
}
bool Mpg123Decoder::isOpen() const
{
    return m_handle != nullptr;
}
const QString &Mpg123Decoder::errorString() const
{
    return m_errorString;
}
int Mpg123Decoder::errorCode() const
{
    return m_errorCode;
}
void Mpg123Decoder::close()
{
    if (m_handle)
    {
        mpg123_close(m_handle);
        mpg123_delete(m_handle);
        m_handle = nullptr;
    }
    m_format = AudioFormat();
    m_durationMs = -1;
}

// 从 libmpg123 读取并验证当前 PCM 输出格式，如果是 s16 则返回 true
bool Mpg123Decoder::updateFormat()
{
    long sampleRate = 0;
    int channels = 0;
    int encoding = 0;
    const int result = mpg123_getformat(m_handle, &sampleRate, &channels, &encoding);
    if (result != MPG123_OK)
    {
        setHandleError(result);
        return false;
    }
    if (sampleRate <= 0 ||
        sampleRate > std::numeric_limits<int>::max())
    {
        setError(-1, QStringLiteral("MP3 采样率无效"));
        return false;
    }

    if (encoding != MPG123_ENC_SIGNED_16) // 固定在 S16 格式
    {
        setError(-1, QStringLiteral("MP3 输出格式不是 S16 PCM"));
        return false;
    }

    m_format.sampleRate = static_cast<int>(sampleRate);
    m_format.channels = channels;
    m_format.sampleFormat = AudioFormat::SampleFormat::SignedInt16;

    if (!m_format.isValid())
    {
        setError(-1, QStringLiteral("MP3 声道数或 PCM 格式不受支持"));
        return false;
    }

    return true;
}

void Mpg123Decoder::setHandleError(int code)
{
    m_errorCode = code;
    if (m_handle)
    {
        m_errorString = QString::fromLocal8Bit(mpg123_strerror(m_handle));
        return;
    }
    m_errorString = QString::fromLocal8Bit(mpg123_plain_strerror(code));
}
// 保存错误码和说明
void Mpg123Decoder::setError(int code, const QString &message)
{
    m_errorCode = code;
    m_errorString = message;
}
void Mpg123Decoder::clearError()
{
    m_errorCode = 0;
    m_errorString.clear();
}
