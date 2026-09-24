#ifndef MPG123DECODER_H
#define MPG123DECODER_H

#include "audio/AudioFormat.h"

#include <QString>
#include <QtGlobal>

#include <mpg123.h>

class Mpg123Decoder final
{
public:
    /** @brief 一次解码调用产生的 PCM 和结束状态。 */
    struct readResult
    {
        qint64 frameCnt = 0;
        bool endOfStream = false;
        bool formatChanged = false;
        bool hasError = false;
    };

    Mpg123Decoder();
    ~Mpg123Decoder();
    Q_DISABLE_COPY(Mpg123Decoder);
    bool open(const QString &path);
    readResult readFrame(qint16 *pcm,
                         qint64 capacityFrames); // 解码PCM帧
    qint64 seekMs(qint64 targets);               // 跳转ms
    qint64 durationMs();                         // MP3总时长(ms)

    AudioFormat format() const;
    bool isOpen() const;
    int errorCode() const;
    const QString &errorString() const;
    void close();

private:
    bool updateFormat();           // 验证PCM
    void setHandleError(int code); // mpg123产生的错误
    void setError(int code, const QString &message);
    void clearError();

    mpg123_handle *m_handle = nullptr;
    AudioFormat m_format;
    qint64 m_durationMs = -1;
    int m_errorCode = 0;
    QString m_errorString;
};

#endif /* MPG123DECODER_H */
