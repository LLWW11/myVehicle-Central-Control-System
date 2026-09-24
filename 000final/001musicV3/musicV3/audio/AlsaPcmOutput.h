#ifndef AlsaPcmOutput_H
#define AlsaPcmOutput_H

#include "audio/AudioFormat.h"

#include <QString>
#include <QtGlobal>

#include <alsa/asoundlib.h>

enum class AlsaWriteStatus : quint8 // 单次写入的状态
{
    Written,    // 写入
    WouldBlock, // 声卡缓冲区暂时不可写
    Recovered,  // ALSA 欠载后已恢复，播放线程必须重建位置
    Error       // 出错
};
struct AlsaWriteResult // 单次写入的结果
{
    /** @brief 兼容 C++11 的写入结果构造函数。 */
    AlsaWriteResult(AlsaWriteStatus value = AlsaWriteStatus::WouldBlock,
                    qint64 frames = 0)
        : status(value), frameWritten(frames) {}
    AlsaWriteStatus status;
    qint64 frameWritten;
};
enum class AlsaWaitResult : quint8 // 等待声卡可写的结果
{
    Ready,    // 可以继续写入
    TimedOut, // 等待超时
    Recovered,// 设备恢复后需要重新同步 PCM
    Error     // 不可恢复错误
};
enum class AlsaDrainResult : quint8 // 声卡缓冲区是否全部放
{
    Finished,   // 所有已提交的 PCM 均播放完毕
    InProgress, // 仍有 PCM 在声卡队列中
    Error       // 排空过程失败
};
// 类独占一个 snd_pcm_t
class AlsaPcmOutput final
{
public:
    AlsaPcmOutput();
    ~AlsaPcmOutput();
    Q_DISABLE_COPY(AlsaPcmOutput);

    // 打开ALSA设备并设为 S16_LE
    bool open(const QString &dev, const AudioFormat &format);

    // 写 framecnt 帧 PCM
    AlsaWriteResult writeSome(const qint16 *interleavedPCM, qint64 framecnt);
    AlsaWaitResult waitWritable(int timeoutMs); // 最多等待timeoutMs缓冲区出现可写
    bool dropAndPrepare();                      // 用于切歌、跳转隐藏窗口和主动暂停播放
    AlsaDrainResult drainStep();                // 清空PCM缓冲区
    qint64 delayFrames() const;                 // 已经进入声卡但是没有播放的帧数，失败返回-1
    void close();
    qint64 periodFrames() const; // Alsa实际输出的周期和缓冲区
    qint64 bufferFrames() const;
    int errorCode() const;
    const QString &errorString() const;

private:
    bool configureHardware(const AudioFormat &format);
    bool configureSoftware();
    bool recover(int alsaError); // 尝试恢复一些可恢复错误(underrun,suspend)
    void setAlsaError(const QString &operation, int alsaError);
    void setError(int code, const QString &message);
    void clearError();

    snd_pcm_t *m_pcm = nullptr;
    AudioFormat m_format;
    snd_pcm_uframes_t m_periodFrames = 0;
    snd_pcm_uframes_t m_bufferFrames = 0;

    int m_errorCode = 0;
    QString m_errorString;
};
#endif /* AlsaPcmOutput_H */
