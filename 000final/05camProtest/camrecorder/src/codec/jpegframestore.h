#ifndef JPEGFRAMESTORE_H
#define JPEGFRAMESTORE_H

#include <QByteArray>
#include <QMutex>
#include <QWaitCondition>

struct JpegFrame
{
    QByteArray bytes;

    int width = 0;
    int height = 0;

    quint64 generation = 0;

    bool isValid() const
    { // 检查是否是有效的JPEG帧
        if (width <= 0 || height <= 0)
            return false;

        if (bytes.size() < 4)
            return false;

        const unsigned char first0 =
            static_cast<unsigned char>(bytes[0]);

        const unsigned char first1 =
            static_cast<unsigned char>(bytes[1]);

        return first0 == 0xff && first1 == 0xd8;
    }
};

class JpegFrameStore
{
public:
    // 发布一个新帧，大小为 bytesPerLine × width × height
    quint64 publish(const QByteArray &jpeg,
                    int width,
                    int height);

    JpegFrame latest() const;

    bool waitForNewFrame(quint64 previousGeneration,
                         int timeoutMs,
                         JpegFrame *frame);

    void clear();

    void wakeAll();

private:
    mutable QMutex m_mutex;
    QWaitCondition m_frameArrived;

    JpegFrame m_latestFrame;

    quint64 m_generation = 0;
};

#endif