#ifndef JPEGENCODERWORKER_H
#define JPEGENCODERWORKER_H

#include <QAtomicInt>
#include <QObject>
#include <QString>
#include <QThread>

class FrameStore;
class JpegFrameStore;

/**
 * @brief 从 FrameStore 取 RGB565 帧编码为 JPEG 并发布到 JpegFrameStore 的线程
 */
class JpegEncoderWorker : public QThread // 编码线程
{
    Q_OBJECT

public:
    JpegEncoderWorker(FrameStore *rawStore,
                      JpegFrameStore *jpegStore,
                      QObject *parent = nullptr);
    ~JpegEncoderWorker() override;

    void requestStop();

Q_SIGNALS:
    void encoderError(const QString &message);

protected:
    void run() override;

private:
    FrameStore *m_rawStore = nullptr;
    JpegFrameStore *m_jpegStore = nullptr;

    QAtomicInt m_stopRequested{0};
};

#endif // JPEGENCODERWORKER_H
